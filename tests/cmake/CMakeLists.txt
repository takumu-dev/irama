# SPDX-FileCopyrightText: 2026 takumu-dev
# SPDX-License-Identifier: Zlib

# The configuration's own behaviour, tested by running its checks as scripts.

# Adds a test that runs a floor check through check_floor.cmake with the definitions in ARGN. A
# toolchain below its floor must stop the configuration with a message naming the floor
# (FAILS_WITH); one at its floor must pass silently; one Irama doesn't support must pass with a
# warning (WARNS_WITH).
function(irama_add_floor_test name)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "FAILS_WITH;WARNS_WITH" "")
    set(test_name "build: ${name}")
    set(definitions "")
    foreach(definition IN LISTS arg_UNPARSED_ARGUMENTS)
        list(APPEND definitions "-D${definition}")
    endforeach()
    add_test(NAME "${test_name}"
        COMMAND "${CMAKE_COMMAND}" ${definitions} -P "${CMAKE_CURRENT_SOURCE_DIR}/check_floor.cmake"
    )
    set_tests_properties("${test_name}" PROPERTIES LABELS unit)
    if(DEFINED arg_FAILS_WITH)
        # CTest passes on the message alone, whatever the exit status, and only FATAL_ERROR prints
        # it.
        set_tests_properties("${test_name}" PROPERTIES
            PASS_REGULAR_EXPRESSION "CMake Error.*${arg_FAILS_WITH}"
        )
    elseif(DEFINED arg_WARNS_WITH)
        set_tests_properties("${test_name}" PROPERTIES
            PASS_REGULAR_EXPRESSION "CMake Warning.*${arg_WARNS_WITH}"
            FAIL_REGULAR_EXPRESSION "CMake Error"
        )
    else()
        set_tests_properties("${test_name}" PROPERTIES
            FAIL_REGULAR_EXPRESSION "CMake (Error|Warning)"
        )
    endif()
endfunction()

irama_add_floor_test("configuring with GCC 15 fails naming the floor"
    COMPILER_ID=GNU COMPILER_VERSION=15.2.0
    FAILS_WITH "Irama needs GCC 16 or newer; this is GCC 15\\.2\\.0"
)
irama_add_floor_test("configuring with GCC 16 passes" COMPILER_ID=GNU COMPILER_VERSION=16.0.0)
irama_add_floor_test("configuring with Clang 22 fails naming the floor"
    COMPILER_ID=Clang COMPILER_VERSION=22.1.8
    FAILS_WITH "Irama needs Clang 23 or newer; this is Clang 22\\.1\\.8"
)
irama_add_floor_test("configuring with Clang 23 passes" COMPILER_ID=Clang COMPILER_VERSION=23.0.0)
irama_add_floor_test("configuring with MSVC 14.51 fails naming the floor"
    COMPILER_ID=MSVC COMPILER_VERSION=19.51.36231
    FAILS_WITH "Irama needs MSVC 14\\.52 \\(cl 19\\.52\\) or newer; this is cl 19\\.51\\.36231"
)
irama_add_floor_test("configuring with MSVC 14.52 passes" COMPILER_ID=MSVC COMPILER_VERSION=19.52.0)
irama_add_floor_test("configuring with an unsupported compiler warns, naming the supported ones"
    COMPILER_ID=AppleClang COMPILER_VERSION=21.0.0
    WARNS_WITH "Irama supports GCC, Clang and MSVC; AppleClang 21\\.0\\.0 is untested"
)
irama_add_floor_test("configuring with libstdc++ 15 fails naming the floor"
    LIBSTDCXX_RELEASE=15
    FAILS_WITH "Irama needs libstdc\\+\\+ 16 or newer; this is libstdc\\+\\+ 15"
)
irama_add_floor_test("configuring with libstdc++ 16 passes" LIBSTDCXX_RELEASE=16)
irama_add_floor_test("configuring with another standard library passes" LIBSTDCXX_RELEASE=)
