# SPDX-FileCopyrightText: 2026 takumu-dev
# SPDX-License-Identifier: Zlib

# Irama: libirama, the library that plays songs; irama-cli, which drives it from the command line;
# and Irama Studio, the tracker itself.
#
# Every supported build is a preset in CMakePresets.json, so a developer and CI run the same
# `cmake --workflow --preset <preset>`. Dependencies come only through find_package from the vcpkg
# manifest; the build never downloads anything.
cmake_minimum_required(VERSION 4.4)

# irama.h holds the one version that libirama, irama-cli and the tests share, so the project takes
# it from there rather than keeping a second copy.
set(irama_header "${CMAKE_CURRENT_SOURCE_DIR}/libirama/include/irama/irama.h")
foreach(part IN ITEMS MAJOR MINOR PATCH)
    file(STRINGS "${irama_header}" line REGEX "^#define IRAMA_VERSION_${part} +[0-9]+$")
    if(NOT line MATCHES "([0-9]+)$")
        message(FATAL_ERROR "irama.h has no IRAMA_VERSION_${part}.")
    endif()
    set(irama_version_${part} "${CMAKE_MATCH_1}")
endforeach()

# Each part beside libirama has a switch, on when Irama is the top-level project, that asks vcpkg
# for the part's manifest feature. The toolchain reads the features at project(), so a part's
# switch and its dependencies can't disagree. With every switch off, only libirama builds.
string(COMPARE EQUAL "${CMAKE_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}" irama_is_top_level)
option(IRAMA_BUILD_CLI "Build irama-cli." ${irama_is_top_level})
option(IRAMA_BUILD_STUDIO "Build Irama Studio." ${irama_is_top_level})
option(IRAMA_BUILD_TESTS "Build the tests." ${irama_is_top_level})
foreach(part IN ITEMS cli studio tests)
    string(TOUPPER "${part}" switch)
    if(IRAMA_BUILD_${switch})
        list(APPEND VCPKG_MANIFEST_FEATURES "${part}")
    endif()
endforeach()

# The real-time guard stops the program when code inside a render call allocates or frees memory. It
# replaces the global operator new and operator delete of every program that renders, so only the
# debug presets turn it on, and never a game's build.
option(IRAMA_REALTIME_GUARD "Stop the program when a render call allocates or frees memory." OFF)

project(irama
    VERSION "${irama_version_MAJOR}.${irama_version_MINOR}.${irama_version_PATCH}"
    DESCRIPTION "A music tracker with built-in synthesisers"
    HOMEPAGE_URL "https://github.com/takumu-dev/irama"
    LANGUAGES CXX
)

# A new version in irama.h reconfigures the build, so the project's version follows it.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${irama_header}")

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include(IramaFloors)
include(IramaConventions)
include(IramaComponents)

irama_check_compiler_floor("${CMAKE_CXX_COMPILER_ID}" "${CMAKE_CXX_COMPILER_VERSION}")
irama_detect_libstdcxx_release(irama_libstdcxx_release)
irama_check_libstdcxx_floor("${irama_libstdcxx_release}")

# Irama's own builds take their dependencies from the vcpkg manifest, and check their licences.
if(irama_is_top_level AND VCPKG_MANIFEST_MODE)
    include(IramaLicenses)
endif()

# Before any target, since IRAMA_CLANG_TIDY reaches only the targets added after it.
include(IramaLint)

add_subdirectory(libirama)

if(IRAMA_BUILD_CLI)
    add_subdirectory(cli)
endif()

if(IRAMA_BUILD_STUDIO)
    add_subdirectory(studio)
endif()

if(IRAMA_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
