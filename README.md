# RECAP swallow labeller

![Screenshot](screenshots/screenshot.png)

Application for labelling swallows built with
[Dear ImGui](https://github.com/ocornut/imgui) and
[implot](https://github.com/epezent/implot).

## Building

Requires `cmake`.

### Linux

```
cmake -B build -DCMAKE_BUILD_TYPE=Release .
cmake --build build --parallel
```

Requires `zlib`, `glfw3`, and `opengl3` to be installed. Third party
dependencies are fetched using CMake FetchContent by default; to disable, set
`FETCH_VENDORED` to false. The dependencies can can then be provided by the
system or via vcpkg by ensuring the `vcpkg` subdmodule is cloned and passing
`--toolchain=vcpkg/scripts/buildsystems/vcpkg.cmake` to the `cmake` configure
command.

### Windows (MSVC)

The CMake preset for Windows uses vcpkg, so the `vcpkg` submodule must be
cloned.

```
cmake --preset windows-static
cmake --build build --config Release --parallel
```
