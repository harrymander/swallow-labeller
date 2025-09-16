vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO harrymander/cnpy
    REF cbece62a9e7355c033ab148d8a559363faa14193
    SHA512 "dcf52d5061abe369457276908e5ccc083435f9b0f4d38aae5ead11dd032a97fe8\
f85263100f4b8de5c00e25d9575cefaa3904f130a22f64882c04a422b470a69"
    HEAD_REF master
)

vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

file(
    INSTALL "${CMAKE_CURRENT_LIST_DIR}/usage"
    DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}"
)
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
