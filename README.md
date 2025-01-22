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
cmake --preset windows-static-{msbuild,ninja,ninja-multi} [-DCMAKE_BUILD_TYPE=<build type>]
cmake --build build [--config <build type>] --parallel
```

The `<build-type>` can be `Release`, `Debug`, `RelWithDebInfo`, etc. When using
a single-config generator (`ninja`), you need to specify the build type at
configure time (via the `-DCMAKE_BUILD_TYPE` flag in the first command). When
using a multi-config generator (`msbuild` or `ninja-multi`), you need to specify
the build type in the build command (`--config` flag, second line).

I recommend using one of the Ninja generators for local development on Windows,
as Ninja can *significantly* decrease build times on multi-core machines and
when running incremental builds.

A range of build presets have been provided for if you are using VS Code with
the CMake Tools extension and a multi-config generator. For a single-config
generator, you can just change the `CMAKE_BUILD_TYPE` cache variable in VS Code
or add the `-DCMAKE_BUILD_TYPE=` flag to the `cmake.configureArgs` property
in your workspace `settings.json`.
