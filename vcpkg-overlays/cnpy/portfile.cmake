vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO harrymander/cnpy
    REF 3f2a9f9ba977547710413240760697df325675d4
    SHA512 "d533895ed007181a0ff373fb41dd8660d0e891761309f7e9aba6cec755bc507593\
ee84f1102bd60f767078d12cbf5d067e596f37ad39f334a2fe3b934b824550"
    HEAD_REF master
)

vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()

file(
    INSTALL "${CMAKE_CURRENT_LIST_DIR}/usage"
    DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}"
)
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
