# SPDX-FileCopyrightText: 2026 takumu-dev
# SPDX-License-Identifier: Zlib

# irama-cli as a user runs it, checked through its output.

# Adds a test that runs irama-cli with ARGUMENTS and passes only if it exits 0 having written
# exactly the line EXPECTED.
function(irama_add_cli_test name)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "EXPECTED" "ARGUMENTS")
    set(test_name "cli: ${name}")
    # Each separator written as a generator expression, so that add_test() keeps the list as one
    # argument, and the script gets it with plain separators.
    string(REPLACE ";" "$<SEMICOLON>" arguments "${arg_ARGUMENTS}")
    add_test(NAME "${test_name}"
        COMMAND "${CMAKE_COMMAND}"
            "-DPROGRAM=$<TARGET_FILE:irama-cli>"
            "-DARGUMENTS=${arguments}"
            "-DEXPECTED=${arg_EXPECTED}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/expect_output.cmake"
    )
    set_tests_properties("${test_name}" PROPERTIES LABELS unit)
endfunction()

irama_add_cli_test("--version prints the release version"
    ARGUMENTS --version
    EXPECTED "irama-cli ${PROJECT_VERSION}"
)
