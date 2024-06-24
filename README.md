# RECAP swallow labeller

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

Requires `zlib`, `glfw3`, and `opengl3` to be installed. `zlib` can be provided
via vcpkg by adding `--toolchain vcpkg/scripts/buildsystems/vcpkg.cmake` to the
first configure command, provided that the `vcpkg` submodule is cloned.

### Windows (MSVC)

The following command uses vcpkg to install `zlib`, so the `vcpkg` submodule
must be cloned.

```
cmake --preset windows
cmake --build build --config Release --parallel
```
