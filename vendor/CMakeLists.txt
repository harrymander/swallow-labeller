include(FetchContent)

if(FETCH_VENDORED)
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
        URL
        https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
        URL_HASH SHA256=${json_expected_sha256}
    )
    fetchcontent_makeavailable(json)

    fetchcontent_declare(
        argparse
        GIT_REPOSITORY https://github.com/p-ranav/argparse.git
        GIT_TAG af442b4da0cd7a07b56fa709bd16571889dc7fda # v3.0
    )
    fetchcontent_makeavailable(argparse)

    fetchcontent_declare(
        magic_enum
        GIT_REPOSITORY https://github.com/Neargye/magic_enum
        GIT_TAG dd6a39d0ba1852cf06907e0f0573a2a10d23c2ad # v0.9.6
    )
    fetchcontent_makeavailable(magic_enum)

    fetchcontent_declare(
        nativefiledialog_extended
        GIT_REPOSITORY https://github.com/btzy/nativefiledialog-extended
        GIT_TAG 86d5f2005fe1c00747348a12070fec493ea2407e  # v1.2.1
    )
    fetchcontent_makeavailable(nativefiledialog_extended)
else()
    set(CMAKE_FIND_PACKAGE_TARGETS_GLOBAL ON)

    find_package(argparse CONFIG REQUIRED)
    find_package(fmt CONFIG REQUIRED)
    find_package(magic_enum CONFIG REQUIRED)
    find_package(nfd CONFIG REQUIRED)
    find_package(nlohmann_json CONFIG REQUIRED)
    find_package(spdlog CONFIG REQUIRED)
endif()

# GCC doesn't support C++20 std::chrono::parse, but date::parse doesn't work on
# MSVC. Fortunately, std::chrono::parse works on MSVC; so we use Howard
# Hinnant's date library on GCC and stdlib on MSVC, with the option to manually
# select which library is used
add_library(chrono_util INTERFACE)
if(MSVC)
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
    fetchcontent_declare(howardhinnant_date
      GIT_REPOSITORY https://github.com/HowardHinnant/date.git
      GIT_TAG 6e921e1b1d21e84a5c82416ba7ecd98e33a436d0 # v3.0.1
    )
    if(NOT howardhinnant_date_POPULATED)
        fetchcontent_populate(howardhinnant_date)
        add_subdirectory(
            ${howardhinnant_date_SOURCE_DIR}
            ${howardhinnant_date_BINARY_DIR}
            EXCLUDE_FROM_ALL
        )
    endif()

    target_link_libraries(chrono_util INTERFACE date::date)
    target_compile_definitions(chrono_util INTERFACE USE_HOWARDHINNANT_DATE)
endif()

fetchcontent_declare(
    cnpy
    GIT_REPOSITORY https://github.com/harrymander/cnpy
    GIT_TAG 171dc9e20966e78f132d0b1b6f3c72c3f8dfa2fb
)
fetchcontent_makeavailable(cnpy)

include(imgui.cmake)
include(implot.cmake)
include(source_sans.cmake)
include(fontawesome.cmake)
include(icon-font-headers.cmake)
