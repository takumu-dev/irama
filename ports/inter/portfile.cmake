# SPDX-FileCopyrightText: 2026 takumu-dev
# SPDX-License-Identifier: Zlib

# Inter Regular, the static TTF from the release's archive, and its licence. vcpkg has no port for
# it. A font is data, the same for every build type, so the port installs it once, beside a CMake
# package that gives its path:
#
#   find_package(inter CONFIG REQUIRED)  sets INTER_REGULAR_TTF
set(VCPKG_BUILD_TYPE release)
set(VCPKG_POLICY_EMPTY_INCLUDE_FOLDER enabled)

vcpkg_download_distfile(ARCHIVE
    URLS "https://github.com/rsms/inter/releases/download/v${VERSION}/Inter-${VERSION}.zip"
    FILENAME "inter-${VERSION}.zip"
    SHA512 2ab20c6c1b33a8adc09c8cf10ad00e2b03cc68f9dceec829aaa1e2bd999720528da4c4594abed39818707e35ff63df6330155cb53660e86029410a0ea4079df4
)
vcpkg_extract_source_archive(SOURCE_PATH ARCHIVE "${ARCHIVE}" NO_REMOVE_ONE_LEVEL)

file(INSTALL "${SOURCE_PATH}/extras/ttf/Inter-Regular.ttf"
    DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}"
)
file(WRITE "${CURRENT_PACKAGES_DIR}/share/${PORT}/${PORT}-config.cmake"
    "set(INTER_REGULAR_TTF \"\${CMAKE_CURRENT_LIST_DIR}/Inter-Regular.ttf\")\n"
)
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE.txt")
