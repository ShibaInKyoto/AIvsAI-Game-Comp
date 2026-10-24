# doctest is header-only; SOURCE_SUBDIR points at a directory without a
# CMakeLists.txt so the dependency is downloaded but its build files are not used.
FetchContent_Declare(doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG ae7a13539fb71f270b87eb2e874fbac80bc8dda2 # v2.4.11
    SOURCE_SUBDIR doctest
    SYSTEM)
FetchContent_MakeAvailable(doctest)

file(GLOB GB_TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
add_executable(gridbreaker_tests ${GB_TEST_SOURCES})
target_include_directories(gridbreaker_tests SYSTEM PRIVATE "${doctest_SOURCE_DIR}")
target_link_libraries(gridbreaker_tests PRIVATE gridbreaker_lib)
gb_set_warnings(gridbreaker_tests)
add_test(NAME gridbreaker_tests COMMAND gridbreaker_tests)
