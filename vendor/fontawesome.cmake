include(FetchContent)

set(FONTAWESOME_VERSION 6.5.2)
string(
    JOIN "/" FONTAWESOME_URL
    https://use.fontawesome.com/releases
    v${FONTAWESOME_VERSION}
    fontawesome-free-${FONTAWESOME_VERSION}-desktop.zip
)
fetchcontent_declare(
    fontawesome
    URL "${FONTAWESOME_URL}"
    URL_HASH
        SHA256=6392bc956eb3d391c9d7a14e891ce8010226ffc0c75f1338db126f13cb9cb8f4
)
fetchcontent_makeavailable(fontawesome)
set(
    FONTAWESOME_FREE_SOLID_OTF
    "${fontawesome_SOURCE_DIR}/otfs/Font Awesome 6 Free-Solid-900.otf"
    PARENT_SCOPE
)
