include(FetchContent)
set(
    IMPLOT_INFINITE_LOOP_FIX_PATCH
    0001-Fix-infinite-loop-when-plot-ranges-are-very-small.patch
)
fetchcontent_declare(
    implot
    GIT_REPOSITORY https://github.com/harrymander/implot
    GIT_TAG 15d73277b9f227a57ac2bcfb41311c37677fb0ad
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
