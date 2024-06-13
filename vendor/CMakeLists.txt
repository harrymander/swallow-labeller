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

fetchcontent_declare(
    cnpy
    GIT_REPOSITORY https://github.com/harrymander/cnpy
    GIT_TAG 138c1a6d5478c521130ab38bf73fd382cdb2ab37
)
fetchcontent_makeavailable(cnpy)

include(imgui.cmake)
include(implot.cmake)
