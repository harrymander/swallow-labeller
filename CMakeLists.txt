cmake_minimum_required(VERSION 3.22 FATAL_ERROR)

project(implot-rects LANGUAGES CXX VERSION 0.1.0)

add_subdirectory(vendor)

add_executable(implot-rects
    src/main.cpp
    src/app.cpp
    src/selector.cpp
)
target_compile_features(implot-rects PRIVATE cxx_std_20)
target_link_libraries(implot-rects PRIVATE
    imgui::demo
    imgui::imgui
    imgui::backend::opengl
    imgui::backend::glfw
    implot::demo
    implot::implot
)
target_compile_definitions(implot-rects PRIVATE
    PROGRAM_NAME="${PROJECT_NAME}"
    VERSION_STR="${PROJECT_VERSION}"
)
