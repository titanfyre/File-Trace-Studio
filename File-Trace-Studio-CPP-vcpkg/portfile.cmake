vcpkg_download_distfile(ARCHIVE
    URLS "https://github.com/titanfyre/File-Trace-Studio/releases/download/sdk-v0.1.0/file-trace-studio-sdk-0.1.0.tar.gz"
    FILENAME "file-trace-studio-sdk-0.1.0.tar.gz"
    SHA512 5c4710af05e60cdc723692e4ab3c87360cae350fe8b4771df6919c645c2bc2a71ed618a67b72afc7f867661e6c59b446b22e9a9e628a8813a7bfac94a3faca48
)
vcpkg_extract_source_archive(SOURCE_PATH ARCHIVE "${ARCHIVE}" NO_REMOVE_ONE_LEVEL)
vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME FileTraceStudioSDK CONFIG_PATH lib/cmake/FileTraceStudioSDK)
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include" "${CURRENT_PACKAGES_DIR}/debug/share")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
