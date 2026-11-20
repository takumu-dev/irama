# SPDX-FileCopyrightText: 2026 takumu-dev
# SPDX-License-Identifier: Zlib

# irama-cli as a user runs it, checked through its output.

# Adds a test that runs irama-cli with ARGUMENTS and passes only if it exits 0 having written
# exactly the line EXPECTED, or, given EXPECTED_ERROR instead, only if it refuses its command line,
# having written exactly the lines EXPECTED_ERROR to stderr.
function(irama_add_cli_test name)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "EXPECTED;EXPECTED_ERROR" "ARGUMENTS")
    set(test_name "cli: ${name}")
    # Each separator written as a generator expression, so that add_test() keeps the list as one
    # argument, and the script gets it with plain separators.
    string(REPLACE ";" "$<SEMICOLON>" arguments "${arg_ARGUMENTS}")
    if(DEFINED arg_EXPECTED_ERROR)
        set(expected "-DEXPECTED_ERROR=${arg_EXPECTED_ERROR}")
    else()
        set(expected "-DEXPECTED=${arg_EXPECTED}")
    endif()
    add_test(NAME "${test_name}"
        COMMAND "${CMAKE_COMMAND}"
            "-DPROGRAM=$<TARGET_FILE:irama-cli>"
            "-DARGUMENTS=${arguments}"
            "${expected}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/expect_output.cmake"
    )
    set_tests_properties("${test_name}" PROPERTIES LABELS unit)
endfunction()

irama_add_cli_test("--version prints the release version"
    ARGUMENTS --version
    EXPECTED "irama-cli ${PROJECT_VERSION}"
)

set(usage "usage: irama-cli --version\n       irama-cli tone <file.wav> [--freq <hertz>] [--seconds <seconds>]")
irama_add_cli_test("tone without a file prints the usage"
    ARGUMENTS tone
    EXPECTED_ERROR "${usage}"
)
irama_add_cli_test("tone with two files prints the usage"
    ARGUMENTS tone first.wav second.wav
    EXPECTED_ERROR "${usage}"
)
irama_add_cli_test("tone with an option it doesn't know prints the usage"
    ARGUMENTS tone out.wav --rate 44100
    EXPECTED_ERROR "${usage}"
)
irama_add_cli_test("tone refuses a frequency at the Nyquist frequency"
    ARGUMENTS tone out.wav --freq 24000
    EXPECTED_ERROR "irama-cli: --freq takes a number of hertz above 0 and below 24000"
)
irama_add_cli_test("tone refuses a frequency that isn't a number"
    ARGUMENTS tone out.wav --freq 440Hz
    EXPECTED_ERROR "irama-cli: --freq takes a number of hertz above 0 and below 24000"
)
irama_add_cli_test("tone refuses a length of no seconds"
    ARGUMENTS tone out.wav --seconds 0
    EXPECTED_ERROR "irama-cli: --seconds takes a number of seconds above 0, up to 3600"
)
irama_add_cli_test("tone refuses --seconds without a number"
    ARGUMENTS tone out.wav --seconds
    EXPECTED_ERROR "irama-cli: --seconds takes a number of seconds above 0, up to 3600"
)
