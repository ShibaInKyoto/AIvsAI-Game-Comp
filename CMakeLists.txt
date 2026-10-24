cmake_minimum_required(VERSION 3.21)

project(Gridbreaker
    VERSION 1.0.0
    DESCRIPTION "Neon twin-stick roguelite arena shooter"
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Ship a single self-contained executable on Windows: no VC++ redistributable needed.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

option(GB_BUILD_TESTS "Build the unit test executable" ON)
option(GB_WARNINGS_AS_ERRORS "Treat compiler warnings as errors for game code" OFF)

include(FetchContent)

# --- raylib ---------------------------------------------------------------
# Pinned to an exact release. Set FETCHCONTENT_SOURCE_DIR_RAYLIB to use a local checkout.
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(CUSTOMIZE_BUILD OFF CACHE BOOL "" FORCE)
FetchContent_Declare(raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG c1ab645ca298a2801097931d1079b10ff7eb9df8 # 5.5
    GIT_SHALLOW FALSE
    SYSTEM)
FetchContent_MakeAvailable(raylib)

# --- Embedded assets ------------------------------------------------------
# Every asset is compiled into the executable so the game ships as one file.
set(GB_ASSET_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/assets")
file(GLOB_RECURSE GB_ASSET_FILES CONFIGURE_DEPENDS
    "${GB_ASSET_ROOT}/fonts/*.ttf"
    "${GB_ASSET_ROOT}/shaders/*.glsl")
set(GB_ASSET_RELATIVE "")
foreach(asset IN LISTS GB_ASSET_FILES)
    file(RELATIVE_PATH rel "${GB_ASSET_ROOT}" "${asset}")
    list(APPEND GB_ASSET_RELATIVE "${rel}")
endforeach()
set(GB_EMBED_CPP "${CMAKE_CURRENT_BINARY_DIR}/generated/EmbeddedAssetData.cpp")
string(REPLACE ";" "|" GB_ASSET_ARG "${GB_ASSET_RELATIVE}")
add_custom_command(
    OUTPUT "${GB_EMBED_CPP}"
    COMMAND "${CMAKE_COMMAND}"
        "-DASSET_ROOT=${GB_ASSET_ROOT}"
        "-DASSET_FILES=${GB_ASSET_ARG}"
        "-DOUTPUT_CPP=${GB_EMBED_CPP}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateEmbeddedAssets.cmake"
    DEPENDS ${GB_ASSET_FILES} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateEmbeddedAssets.cmake"
    COMMENT "Embedding game assets"
    VERBATIM)

# --- Build metadata -------------------------------------------------------
find_package(Git QUIET)
set(GB_GIT_HASH "unknown")
if(GIT_FOUND)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" rev-parse --short HEAD
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        OUTPUT_VARIABLE GB_GIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
endif()
configure_file(src/core/BuildInfo.h.in "${CMAKE_CURRENT_BINARY_DIR}/generated/core/BuildInfo.h" @ONLY)

# --- Game library (everything except main, so tests can link it) ---------
file(GLOB_RECURSE GB_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h")
list(REMOVE_ITEM GB_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")

add_library(gridbreaker_lib STATIC ${GB_SOURCES} "${GB_EMBED_CPP}")
target_include_directories(gridbreaker_lib PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(gridbreaker_lib PUBLIC raylib)
target_compile_features(gridbreaker_lib PUBLIC cxx_std_20)
target_compile_definitions(gridbreaker_lib PUBLIC RAYMATH_DISABLE_CPP_OPERATORS)

function(gb_set_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
        target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
        if(GB_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wno-missing-field-initializers)
        if(GB_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()
gb_set_warnings(gridbreaker_lib)

if(MINGW)
    target_link_options(gridbreaker_lib PUBLIC -static -static-libgcc -static-libstdc++)
endif()

# --- Executable -----------------------------------------------------------
set(GB_EXE_SOURCES src/main.cpp)
if(WIN32)
    list(APPEND GB_EXE_SOURCES platform/windows/gridbreaker.rc)
endif()
add_executable(gridbreaker WIN32 ${GB_EXE_SOURCES})
target_link_libraries(gridbreaker PRIVATE gridbreaker_lib)
gb_set_warnings(gridbreaker)
set_target_properties(gridbreaker PROPERTIES
    OUTPUT_NAME "Gridbreaker"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin")

# --- Tests ----------------------------------------------------------------
if(GB_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# --- Tools ----------------------------------------------------------------
option(GB_BUILD_TOOLS "Build developer tools (balance simulator)" ON)
if(GB_BUILD_TOOLS)
    add_executable(gridbreaker_balance tools/BalanceSim.cpp)
    target_link_libraries(gridbreaker_balance PRIVATE gridbreaker_lib)
    gb_set_warnings(gridbreaker_balance)
endif()
