include(FetchContent)

fetchcontent_declare(
    fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt
    GIT_TAG e69e5f977d458f2650bb346dadf2ad30c5320281 # 10.2.1
)
fetchcontent_getproperties(fmt)
if(NOT fmt_POPULATED)
    fetchcontent_populate(fmt)
    add_subdirectory(${fmt_SOURCE_DIR} ${fmt_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

set(SPDLOG_FMT_EXTERNAL ON)
fetchcontent_declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog
    GIT_TAG 27cb4c76708608465c413f6d0e6b8d99a4d84302 # 1.14.1
)
fetchcontent_makeavailable(spdlog)

set(
    json_expected_sha256
    d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d
)
fetchcontent_declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    URL_HASH SHA256=${json_expected_sha256}
)
fetchcontent_makeavailable(json)

fetchcontent_declare(
    cnpy
    GIT_REPOSITORY https://github.com/harrymander/cnpy
    GIT_TAG 138c1a6d5478c521130ab38bf73fd382cdb2ab37
)
fetchcontent_makeavailable(cnpy)

fetchcontent_declare(
    argparse
    GIT_REPOSITORY https://github.com/p-ranav/argparse.git
    GIT_TAG af442b4da0cd7a07b56fa709bd16571889dc7fda # v3.0
)
fetchcontent_makeavailable(argparse)

include(imgui.cmake)
include(implot.cmake)
