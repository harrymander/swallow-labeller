include(FetchContent)

find_package(ZLIB REQUIRED)

fetchcontent_declare(
    cnpy
    GIT_REPOSITORY https://github.com/rogersce/cnpy
    GIT_TAG 4e8810b1a8637695171ed346ce68f6984e585ef4
)
fetchcontent_getproperties(cmark)
if(NOT cnpy_POPULATED)
    fetchcontent_populate(cnpy)
    add_subdirectory(${cnpy_SOURCE_DIR} ${cnpy_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

add_library(cnpy-vendored INTERFACE)
target_link_libraries(cnpy-vendored INTERFACE cnpy-static ${ZLIB_LIBRARIES})
target_include_directories(cnpy-vendored INTERFACE ${cnpy_SOURCE_DIR})
