include(FetchContent)
fetchcontent_declare(
    implot
    GIT_REPOSITORY https://github.com/epezent/implot
    GIT_TAG 18c72431f8265e2b0b5378a3a73d8a883b2175ff
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
