include(FetchContent)
set(
    source_sans_url_base
    "https://github.com/adobe-fonts/source-sans/releases/download"
)
fetchcontent_declare(
    source_sans
    URL "${source_sans_url_base}/3.052R/TTF-source-sans-3.052R.zip"
    URL_HASH MD5=11faea909c263b00ffe17e033250b083
)
fetchcontent_makeavailable(source_sans)
set(source_sans_SOURCE_DIR ${source_sans_SOURCE_DIR} PARENT_SCOPE)
set(
    ADOBE_SOURCE_SANS_TTF
    ${source_sans_SOURCE_DIR}/TTF/SourceSans3-Regular.ttf
    PARENT_SCOPE
)
