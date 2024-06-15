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
    set_target_properties(implot-rects PROPERTIES WIN32_EXECUTABLE true)
    target_compile_options(implot-rects PUBLIC /nologo /W4)
    target_compile_definitions(implot-rects PUBLIC UNICODE _UNICODE)
else()
    message(FATAL_ERROR "Unsupported platform")
endif()


find_program(CPPCHECK cppcheck)
if(CPPCHECK STREQUAL CPPCHECK-NOTFOUND)
    message(WARNING "cppcheck not found, disabling check target")
else()
    add_custom_target(
        check
        COMMAND
            ${CPPCHECK} --error-exitcode=1 --force --quiet
                        --enable=style --language=c++ --std=c++20
                        ${CMAKE_CURRENT_SOURCE_DIR}/src
                        ${CMAKE_CURRENT_SOURCE_DIR}/tests
        COMMENT "Checking source code with cppcheck"
    )
endif()

find_program(CLANG_FORMAT clang-format REQUIRED)
if(CLANG_FORMAT STREQUAL CLANG_FORMAT-NOTFOUND)
    message(WARNING "clang-format not found, disabling lint and format targets")
else()
    set(
        CLANG_FORMAT_CMD
        ${CLANG_FORMAT} -Werror
            ${CMAKE_CURRENT_SOURCE_DIR}/src/*
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp
    )

    add_custom_target(
        lint
        COMMAND ${CLANG_FORMAT_CMD} --dry-run
        COMMENT "Linting source code with clang-format"
    )
    if(TARGET check)
        add_custom_target(lint-all DEPENDS check lint)
    endif()

    add_custom_target(
        format
        COMMAND ${CLANG_FORMAT_CMD} -i
        COMMENT "Formatting source code with clang-format"
    )
endif()
