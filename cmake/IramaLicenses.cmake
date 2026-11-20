# SPDX-FileCopyrightText: 2026 takumu-dev
# SPDX-License-Identifier: Zlib

# The licences of Irama's dependencies, for a build that takes them from the vcpkg manifest.
# Configuring checks every port vcpkg installed, build tools included, against the allowlists in
# cmake/licenses.json, and stops on any that its list doesn't cover.
#
#   notices  writes THIRD_PARTY_NOTICES.md for what the applications ship, from this build's tree
#
# IRAMA_NOTICES_OPTIONS holds the options that check_licenses.cmake takes to work out the notices,
# for the test that the committed file is up to date.

include_guard(GLOBAL)

set(irama_license_check "${CMAKE_CURRENT_LIST_DIR}/check_licenses.cmake")
set(irama_license_options
    "-DINSTALLED_DIR=${VCPKG_INSTALLED_DIR}"
    "-DMANIFEST=${PROJECT_SOURCE_DIR}/vcpkg.json"
    "-DTRIPLET=${VCPKG_TARGET_TRIPLET}"
)
# The applications that ship, each with the manifest feature that holds its dependencies.
set(IRAMA_NOTICES_OPTIONS ${irama_license_options} "-DAPPLICATIONS=cli=irama-cli")

execute_process(
    COMMAND "${CMAKE_COMMAND}" ${irama_license_options} -P "${irama_license_check}"
    RESULT_VARIABLE irama_license_result
)
if(NOT irama_license_result EQUAL 0)
    message(FATAL_ERROR "The licence check above failed.")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/licenses.json"
    "${irama_license_check}"
)

add_custom_target(notices
    COMMAND "${CMAKE_COMMAND}" ${IRAMA_NOTICES_OPTIONS}
        "-DWRITE_NOTICES=${PROJECT_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
        -P "${irama_license_check}"
    COMMENT "Writing THIRD_PARTY_NOTICES.md"
    VERBATIM
)
