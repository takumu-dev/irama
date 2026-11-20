# SPDX-FileCopyrightText: 2026 takumu-dev
# SPDX-License-Identifier: Zlib

# dr_libs at a commit with dr_flac 0.13.4 and the fixes to its parser's bounds checks that followed
# it. It stands in for vcpkg's own port, which is at an older commit, until that port reaches this
# one. The libraries are single headers, so the port installs the headers and the licence alone.
vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO mackron/dr_libs
    REF e381981afa52800b9a3864df548353ff67252fbc
    SHA512 dc00ad9ca63a3c77247023ded358548e8b260c5585c1a8ef616dd84517f05b5180a41016eba9b0b8020665d811c22161d67738560515d4778bcca2473e5cfb71
    HEAD_REF master
)

file(GLOB headers "${SOURCE_PATH}/*.h")
file(INSTALL ${headers} DESTINATION "${CURRENT_PACKAGES_DIR}/include")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
