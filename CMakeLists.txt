cmake_minimum_required(VERSION 3.25 FATAL_ERROR)

project(recap-labeller LANGUAGES CXX VERSION 0.1.0)

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

set(SOURCE_SANS_CPP_FILE ${CMAKE_CURRENT_BINARY_DIR}/gen/source-sans.cpp)
imgui_compressed_c_array(
    ${source_sans_SOURCE_DIR}/TTF/SourceSans3-Regular.ttf
    ${SOURCE_SANS_CPP_FILE}
    adobe_source_sans
)

set(FONTAWESOME_CPP_FILE ${CMAKE_CURRENT_BINARY_DIR}/gen/fontawesome.cpp)
imgui_compressed_c_array(
    ${FONTAWESOME_FREE_SOLID_OTF}
    ${FONTAWESOME_CPP_FILE}
    fontawesome_free_solid
)

add_executable(recap-labeller
    ${FONTAWESOME_CPP_FILE}
    ${SOURCE_SANS_CPP_FILE}
    src/annotation-manager.cpp
    src/app.cpp
    src/data.cpp
    src/gui/font.cpp
    src/gui/gui.cpp
    src/gui/widgets/plot-range.cpp
    src/gui/widgets/plot-range-dragger.cpp
    src/gui/widgets/plot-range-maker.cpp
    src/gui/widgets/util.cpp
    src/labelling-task.cpp
    src/main.cpp
)
target_compile_features(recap-labeller PRIVATE cxx_std_20)
target_link_libraries(recap-labeller PRIVATE
    argparse
    cnpy::cnpy
    fmt::fmt
    IconFontCppHeaders
    imgui::demo
    imgui::imgui
    implot::demo
    implot::implot
    nlohmann_json::nlohmann_json
    spdlog::spdlog
)
target_compile_definitions(recap-labeller PRIVATE
    PROGRAM_NAME="${PROJECT_NAME}"
    VERSION_STR="${PROJECT_VERSION}"
)
if(NOT MSVC)
    target_compile_options(recap-labeller PRIVATE -Wall -Wextra)
endif()

if(LINUX)
    target_link_libraries(
        recap-labeller
        PRIVATE imgui::backend::opengl imgui::backend::glfw
    )
    target_sources(recap-labeller PRIVATE src/platform-glfw-opengl3.cpp)
elseif(WIN32 AND MSVC)
    target_sources(recap-labeller PUBLIC src/platform-win32-dx12.cpp)
    target_link_libraries(recap-labeller PUBLIC imgui::backend::win32-native)
    set_target_properties(recap-labeller PROPERTIES WIN32_EXECUTABLE false)
    target_compile_options(recap-labeller PUBLIC /nologo /W4)
    target_compile_definitions(recap-labeller PUBLIC UNICODE _UNICODE)
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

option(ENABLE_LINTING "Enable linting" ON)
if(ENABLE_LINTING)
    include(cmake/lint.cmake)
endif()
