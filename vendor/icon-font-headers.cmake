include(FetchContent)

fetchcontent_declare(
    iconfontcppheaders
    GIT_REPOSITORY https://github.com/juliettef/IconFontCppHeaders.git
    GIT_TAG 62d27fa93d8f1f881dac18f13881dd97af66fa74
)
fetchcontent_makeavailable(iconfontcppheaders)

add_library(IconFontCppHeaders INTERFACE)
target_include_directories(
    IconFontCppHeaders INTERFACE ${iconfontcppheaders_SOURCE_DIR}
)
