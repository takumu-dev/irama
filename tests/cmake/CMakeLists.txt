# SPDX-FileCopyrightText: 2026 takumu-dev
# SPDX-License-Identifier: Zlib

# The configuration's own behaviour, tested by running its checks as scripts, or by configuring
# small projects that use them.

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
irama_add_floor_test("configuring with clang-format 22 fails naming the floor"
    TOOL=clang-format TOOL_VERSION=22.1.8
    FAILS_WITH "Irama needs clang-format 23 or newer; this is clang-format 22\\.1\\.8"
)
irama_add_floor_test("configuring with clang-format 23 passes"
    TOOL=clang-format TOOL_VERSION=23.0.0
)
irama_add_floor_test("configuring with clang-tidy 22 fails naming the floor"
    TOOL=clang-tidy TOOL_VERSION=22.1.8
    FAILS_WITH "Irama needs clang-tidy 23 or newer; this is clang-tidy 22\\.1\\.8"
)
irama_add_floor_test("configuring with clang-tidy 23 passes" TOOL=clang-tidy TOOL_VERSION=23.0.0)
irama_add_floor_test("configuring with Doxygen 1.17 fails naming the floor"
    TOOL=Doxygen TOOL_VERSION=1.17.0
    FAILS_WITH "Irama needs Doxygen 1\\.18 or newer; this is Doxygen 1\\.17\\.0"
)
irama_add_floor_test("configuring with Doxygen 1.18 passes" TOOL=Doxygen TOOL_VERSION=1.18.0)

# The link-closure check, run by configuring the project in link_closure/ with its CASE. The
# configuration must pass, or fail with a message matching FAILS_WITH.
function(irama_add_link_closure_test name case)
    cmake_parse_arguments(PARSE_ARGV 2 arg "" "FAILS_WITH" "")
    set(test_name "build: ${name}")
    add_test(NAME "${test_name}"
        COMMAND "${CMAKE_COMMAND}" --fresh
            -S "${CMAKE_CURRENT_SOURCE_DIR}/link_closure"
            -B "${CMAKE_CURRENT_BINARY_DIR}/link_closure/${case}"
            -G "${CMAKE_GENERATOR}"
            "-DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}"
            "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
            "-DCASE=${case}"
    )
    set_tests_properties("${test_name}" PROPERTIES LABELS unit)
    if(DEFINED arg_FAILS_WITH)
        set_tests_properties("${test_name}" PROPERTIES
            PASS_REGULAR_EXPRESSION "CMake Error.*${arg_FAILS_WITH}"
        )
    else()
        set_tests_properties("${test_name}" PROPERTIES FAIL_REGULAR_EXPRESSION "CMake Error")
    endif()
endfunction()

irama_add_link_closure_test(
    "libirama targets that link one another and the imported targets allowed pass the check"
    allowed
)
irama_add_link_closure_test(
    "a libirama target that links an imported target not allowed fails the check, naming the link"
    imported
    FAILS_WITH "irama_wav links Fake::forbidden\\."
)
irama_add_link_closure_test(
    "a libirama target that links a library by name fails the check, naming the link"
    library
    FAILS_WITH "irama_wav links m\\."
)
irama_add_link_closure_test(
    "a libirama target that links a target from outside libirama fails the check, naming the link"
    target
    FAILS_WITH "irama_wav links project_helper\\."
)
irama_add_link_closure_test(
    "a libirama target that links through a generator expression fails the check, naming it"
    expression
    # Test properties take generator expressions, so the pattern steers clear of writing one.
    FAILS_WITH "irama_wav links [^ ]+CONFIG:Debug.:Fake::allowed., a generator expression"
)
irama_add_link_closure_test(
    "a libirama target that passes on a link not allowed fails the check, naming the link"
    interface
    FAILS_WITH "irama_base links Fake::forbidden\\."
)
irama_add_link_closure_test(
    "a link added to libirama after libirama is configured fails the check, naming it"
    late
    FAILS_WITH "irama_engine links Fake::forbidden\\."
)
