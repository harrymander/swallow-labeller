cmake_minimum_required(VERSION 3.25 FATAL_ERROR)

project(implot-rects LANGUAGES CXX VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
if(WIN32)
    # Disables the min/max preprocessor macros, so not to interfere with
    # min/max in the STL
    add_compile_definitions(NOMINMAX)
endif()

add_subdirectory(vendor)

option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

add_executable(implot-rects
    src/annotation-manager.cpp
    src/data.cpp
    src/gui.cpp
    src/drag-range.cpp
    src/labelling-task.cpp
    src/main.cpp
    src/plotter.cpp
    src/selector.cpp
)
target_compile_features(implot-rects PRIVATE cxx_std_20)
target_link_libraries(implot-rects PRIVATE
    argparse
    cnpy::cnpy
    imgui::demo
    imgui::imgui
    implot::demo
    implot::implot
    nlohmann_json::nlohmann_json
    spdlog::spdlog
)
target_compile_definitions(implot-rects PRIVATE
    PROGRAM_NAME="${PROJECT_NAME}"
    VERSION_STR="${PROJECT_VERSION}"
)
if(NOT MSVC)
    target_compile_options(implot-rects PRIVATE -Wall -Wextra)
endif()

if(LINUX)
    target_link_libraries(
        implot-rects
        PRIVATE imgui::backend::opengl imgui::backend::glfw
    )
    target_sources(implot-rects PRIVATE src/platform-glfw-opengl3.cpp)
elseif(WIN32 AND MSVC)
    target_sources(implot-rects PUBLIC src/platform-win32-dx12.cpp)
    target_link_libraries(implot-rects PUBLIC imgui::backend::win32-native)
    set_target_properties(implot-rects PROPERTIES WIN32_EXECUTABLE false)
    target_compile_options(implot-rects PUBLIC /nologo /W4)
    target_compile_definitions(implot-rects PUBLIC UNICODE _UNICODE)
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

option(ENABLE_LINTING "Enable linting" ON)
if(ENABLE_LINTING)
    include(cmake/lint.cmake)
endif()
