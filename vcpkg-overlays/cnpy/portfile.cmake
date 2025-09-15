vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO harrymander/cnpy
    REF 8c583821d680a7b44f712a81a9615d199430557f
    SHA512 "680f61e77566fb5daf80dcce6313f45800cc3911e4190540618af968d98c56f3e\
acfdaaf6899da1ad8810aa845ca81104405c3458a446cefb38f8df9aa6bff0a"
    HEAD_REF master
)

file(
    INSTALL "${CMAKE_CURRENT_LIST_DIR}/usage"
    DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}"
)
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")

include(CMakePackageConfigHelpers)
configure_package_config_file(
    "${CMAKE_CURRENT_LIST_DIR}/Config.cmake.in"
    "${CURRENT_PACKAGES_DIR}/share/cmake/${PORT}/${PORT}Config.cmake"
    INSTALL_DESTINATION share/cmake/${PORT}
)

vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()
