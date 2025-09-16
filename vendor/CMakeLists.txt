# C++20 provides std::chrono::parse, but full support is not available on some
# compilers that mostly support C++20 (e.g. GCC 13). By default use Howard
# Hinnant's date library with compilers that don't support the stdlib version.
# Note that the Howard Hinnant library may not work on compilers that _do_
# support std::chrono::parse.
add_library(chrono_util INTERFACE)
if(
    MSVC OR (
        (CMAKE_CXX_COMPILER_ID STREQUAL GNU) AND
        (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 14)
    ) OR (
        (CMAKE_CXX_COMPILER_ID STREQUAL Clang) AND
        (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 18)
    )
)
    set(USE_HOWARDHINNANT_DATE_DEFAULT off)
else()
    set(USE_HOWARDHINNANT_DATE_DEFAULT on)
endif()
option(
    USE_HOWARDHINNANT_DATE
    "Use Howard Hinnant's date library instead of stdlib chrono"
    ${USE_HOWARDHINNANT_DATE_DEFAULT}
)
if(USE_HOWARDHINNANT_DATE)
    message("Using Howard Hinnant date library for chrono utils")
    find_package(date CONFIG REQUIRED)
    target_link_libraries(chrono_util INTERFACE date::date)
    target_compile_definitions(chrono_util INTERFACE USE_HOWARDHINNANT_DATE)
else()
    message("Using stdlib chrono utils")
endif()

include(imgui.cmake)
include(implot.cmake)
include(source_sans.cmake)
include(fontawesome.cmake)
include(icon-font-headers.cmake)
