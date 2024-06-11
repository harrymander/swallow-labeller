include(FetchContent)

fetchcontent_declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog
    GIT_TAG 27cb4c76708608465c413f6d0e6b8d99a4d84302 # 1.14.1
)
fetchcontent_makeavailable(spdlog)

fetchcontent_declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
fetchcontent_makeavailable(json)

include(cnpy.cmake)
include(imgui.cmake)
include(implot.cmake)
