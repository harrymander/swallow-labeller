include(FetchContent)
set(
    IMPLOT_INFINITE_LOOP_FIX_PATCH
    0001-Fix-infinite-loop-when-plot-ranges-are-very-small.patch
)
fetchcontent_declare(
    implot
    GIT_REPOSITORY https://github.com/epezent/implot
    GIT_TAG f156599faefe316f7dd20fe6c783bf87c8bb6fd9
    PATCH_COMMAND
        git am ${CMAKE_CURRENT_SOURCE_DIR}/${IMPLOT_INFINITE_LOOP_FIX_PATCH}
)
fetchcontent_makeavailable(implot)

add_library(implot-core INTERFACE)
target_sources(implot-core INTERFACE
    ${implot_SOURCE_DIR}/implot.cpp
    ${implot_SOURCE_DIR}/implot_items.cpp
)
target_include_directories(implot-core INTERFACE ${implot_SOURCE_DIR})
add_library(implot::implot ALIAS implot-core)

add_library(implot-demo INTERFACE)
target_sources(implot-demo INTERFACE ${implot_SOURCE_DIR}/implot_demo.cpp)
add_library(implot::demo ALIAS implot-demo)
