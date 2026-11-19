# SPDX-FileCopyrightText: 2026 takumu-dev
# SPDX-License-Identifier: Zlib

# Runs one of the configuration's floor checks on a toolchain this machine may lack, as CMake would
# report it: the compiler floor on COMPILER_ID and COMPILER_VERSION, or, when LIBSTDCXX_RELEASE is
# defined, the libstdc++ floor on that value of _GLIBCXX_RELEASE.
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/IramaFloors.cmake")

if(DEFINED LIBSTDCXX_RELEASE)
    irama_check_libstdcxx_floor("${LIBSTDCXX_RELEASE}")
else()
    irama_check_compiler_floor("${COMPILER_ID}" "${COMPILER_VERSION}")
endif()
