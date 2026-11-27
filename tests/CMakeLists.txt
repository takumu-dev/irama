# SPDX-FileCopyrightText: 2026 takumu-dev
# SPDX-License-Identifier: Zlib

# Irama's tests. Each one carries exactly one CTest label: `abort` for the checks that end the
# program, run through cmake/expect_abort.cmake, `golden` for the golden cases, run through
# cmake/golden_test.cmake, `smoke` for the test that starts Irama Studio, run through
# cmake/smoke_test.cmake, and `unit` for Catch2 test cases and the other checks run through CMake.
# Each test's name begins with the component it tests, matching the tag on its Catch2 test case, or
# with the part it tests: `build` for the configuration, `cli` for irama-cli, `studio` for Irama
# Studio, `golden` for a golden case, `golden harness` for the golden tests' own checks and
# `test support` for the measurements the tests share.

find_package(Catch2 3 CONFIG REQUIRED)
include(Catch)

# One test source is C, to check that irama.h is C99.
enable_language(C)
irama_check_compiler_floor("${CMAKE_C_COMPILER_ID}" "${CMAKE_C_COMPILER_VERSION}")

# The main of every Catch2 test program, which keeps MSVC's runtime from waiting on a dialog.
add_library(irama_catch_main STATIC catch_main.cpp)
target_link_libraries(irama_catch_main PUBLIC Catch2::Catch2)
irama_target_conventions(irama_catch_main)

# Adds a component's Catch2 test cases as the program irama_<component>_tests, linking only that
# component, so its tests can't lean on the components above it. Each case becomes one CTest test,
# named after the component.
function(irama_add_tests component)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "" "SOURCES")
    set(target irama_${component}_tests)
    add_executable(${target} ${arg_SOURCES})
    target_link_libraries(${target} PRIVATE irama::${component} irama_catch_main)
    irama_target_conventions(${target})
    catch_discover_tests(${target}
        TEST_PREFIX "${component}: "
        PROPERTIES LABELS unit
    )
endfunction()

add_subdirectory(abort)
add_subdirectory(cmake)
add_subdirectory(support)
add_subdirectory(unit/base)
add_subdirectory(unit/capi)
add_subdirectory(unit/filter)
add_subdirectory(unit/param)
add_subdirectory(unit/player)
add_subdirectory(unit/source)
add_subdirectory(unit/voice)
add_subdirectory(unit/wav)

if(IRAMA_BUILD_CLI)
    add_subdirectory(cli)
    add_subdirectory(golden)
endif()

if(IRAMA_BUILD_STUDIO)
    add_subdirectory(studio)
endif()
