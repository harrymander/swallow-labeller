cmake_minimum_required(VERSION 3.25 FATAL_ERROR)

project(implot-rects LANGUAGES CXX VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)

add_subdirectory(vendor)

option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

add_executable(implot-rects
    src/app.cpp
    src/data.cpp
    src/drag-range.cpp
    src/labelling-task.cpp
    src/main.cpp
    src/selector.cpp
)
target_compile_features(implot-rects PRIVATE cxx_std_20)
target_link_libraries(implot-rects PRIVATE
    cnpy::cnpy
    imgui::demo
    imgui::imgui
    imgui::backend::opengl
    imgui::backend::glfw
    implot::demo
    implot::implot
    nlohmann_json::nlohmann_json
    spdlog::spdlog
)
target_compile_definitions(implot-rects PRIVATE
    PROGRAM_NAME="${PROJECT_NAME}"
    VERSION_STR="${PROJECT_VERSION}"
)

find_program(CPPCHECK cppcheck REQUIRED)
add_custom_target(
    check
    COMMAND
        ${CPPCHECK}
        --error-exitcode=1 --enable=style --std=c++20 --force --quiet
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/tests
    COMMENT "Checking source code with cppcheck"
)

find_program(CLANG_FORMAT clang-format REQUIRED)
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

add_custom_target(lint-all DEPENDS check lint)

add_custom_target(
    format
    COMMAND ${CLANG_FORMAT_CMD} -i
    COMMENT "Formatting source code with clang-format"
)
