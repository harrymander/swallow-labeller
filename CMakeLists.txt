cmake_minimum_required(VERSION 3.25 FATAL_ERROR)

# Select MSVC runtime library via CMAKE_MSVC_RUNTIME_LIBRARY
cmake_policy(SET CMP0091 NEW)

project(recap-labeller LANGUAGES CXX VERSION 0.3.0)

# Program options
option(RECAP_LABELLER_LOG_TO_APP_DATA_DIR "Log to app data directory" OFF)

include(CMakePrintHelpers)
cmake_print_variables(RECAP_LABELLER_LOG_TO_APP_DATA_DIR)

set(CMAKE_CXX_STANDARD 20)
if(WIN32)
    # Disables the min/max preprocessor macros, so not to interfere with
    # min/max in the STL
    add_compile_definitions(NOMINMAX)
    if(NOT BUILD_SHARED_LIBS)
        set(
            CMAKE_MSVC_RUNTIME_LIBRARY
            "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
    endif()
endif()
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    option(
        GLIBCXX_DEBUG_MODE
        "Enable libstdc++ debug mode for Debug builds"
        ON
    )
    if(GLIBCXX_DEBUG_MODE)
        add_compile_definitions($<$<CONFIG:Debug>:_GLIBCXX_DEBUG=1>)

        # Enable debug backtrace - only supported on GCC 13+(?). find_library
        # doesn't seem to work with system libraries, so try to compile a C++
        # source file with library linked instead to determine if backtrace is
        # supported
        include(CheckCXXSourceCompiles)
        set(backtrace_lib stdc++_libbacktrace)
        set(CMAKE_REQUIRED_LIBRARIES ${backtrace_lib})
        check_cxx_source_compiles("int main() {}" HAS_BACKTRACE)
        if(HAS_BACKTRACE)
            add_compile_definitions(
                $<$<CONFIG:Debug>:_GLIBCXX_DEBUG_BACKTRACE=1>
            )
            link_libraries(debug ${backtrace_lib})
        endif()
    endif()
endif()

find_package(argparse CONFIG REQUIRED)
find_package(cnpy CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(magic_enum CONFIG REQUIRED)
find_package(nfd CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

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

set(OPTIONS_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/gen/include)
configure_file(src/options.h.in ${OPTIONS_INCLUDE_DIR}/options.h)

add_executable(recap-labeller
    src/app/annotation-store.cpp
    src/app/app.cpp
    src/app/config.cpp
    src/gui/font.cpp
    src/gui/gui.cpp
    src/gui/task-filter.cpp
    src/gui/widgets/combo-forced.cpp
    src/gui/widgets/integer-range-input.cpp
    src/gui/widgets/plot-range-dragger.cpp
    src/gui/widgets/plot-range-selector.cpp
    src/gui/widgets/plot-range.cpp
    src/gui/widgets/util.cpp
    src/main.cpp
    src/models/annotation-json.cpp
    src/models/data.cpp
    src/models/task-info.cpp
    ${FONTAWESOME_CPP_FILE}
    ${SOURCE_SANS_CPP_FILE}
)
target_include_directories(recap-labeller PRIVATE src ${OPTIONS_INCLUDE_DIR})
target_compile_features(recap-labeller PRIVATE cxx_std_20)
target_link_libraries(recap-labeller PRIVATE
    argparse::argparse
    chrono_util
    cnpy::cnpy
    fmt::fmt
    IconFontCppHeaders
    imgui::demo
    imgui::imgui
    implot::demo
    implot::implot
    magic_enum::magic_enum
    nfd::nfd
    nlohmann_json::nlohmann_json
    spdlog::spdlog
)
target_compile_definitions(recap-labeller PRIVATE
    PROGRAM_NAME="${PROJECT_NAME}"
    VERSION_STR="${PROJECT_VERSION}"
)
if(NOT MSVC)
    target_compile_options(recap-labeller PRIVATE -Wall -Wextra -pedantic)
endif()

if(LINUX)
    target_link_libraries(
        recap-labeller
        PRIVATE imgui::backend::opengl imgui::backend::glfw
    )
    target_sources(
        recap-labeller PRIVATE
        src/platform/glfw-opengl3.cpp
        src/util/os-linux.cpp
        src/util/os-posix.cpp
    )
elseif(WIN32 AND MSVC)
    target_sources(
        recap-labeller PRIVATE
        src/platform/win32-dx12.cpp
        src/util/os-win32.cpp
    )
    target_link_libraries(recap-labeller PUBLIC imgui::backend::win32-native)
    set_target_properties(recap-labeller PROPERTIES WIN32_EXECUTABLE false)
    target_compile_options(recap-labeller PUBLIC /nologo /W4)
    target_compile_definitions(recap-labeller PUBLIC UNICODE _UNICODE)
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

install(TARGETS recap-labeller DESTINATION bin)

option(ENABLE_LINTING "Enable linting" ON)
if(ENABLE_LINTING)
    include(cmake/lint.cmake)
endif()

option(BUILD_DEMO "Build demo" ON)
if(BUILD_DEMO)
    add_subdirectory(demo)
endif()
