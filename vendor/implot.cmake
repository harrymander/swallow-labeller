include(FetchContent)
fetchcontent_declare(
    implot
    # v1.0 with my fix for infinite loop on small plot ranges
    GIT_REPOSITORY https://github.com/harrymander/implot
    GIT_TAG ccb09987a565812a8f64c4ad0f29eb755f23d3f6
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
