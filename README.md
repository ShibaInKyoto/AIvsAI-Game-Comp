# GRIDBREAKER

A neon twin-stick roguelite arena shooter built in C++20 with raylib.

> Work in progress — full documentation follows.

## Building

### Windows (Visual Studio 2022 / Build Tools)

```bat
cmake -S . -B build -A x64
cmake --build build --config Release
build\bin\Gridbreaker.exe
```

### Linux

```sh
sudo apt install build-essential cmake ninja-build libx11-dev libxrandr-dev libxinerama-dev \
                 libxcursor-dev libxi-dev libgl1-mesa-dev libasound2-dev
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/bin/Gridbreaker
```

Dependencies (raylib 5.5, doctest) are fetched automatically by CMake.
