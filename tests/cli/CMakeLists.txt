# SPDX-FileCopyrightText: 2026 takumu-dev
# SPDX-License-Identifier: Zlib

# irama-cli as a user runs it, checked through its output.

# Adds a test that runs irama-cli with ARGUMENTS and passes only if it exits 0 having written
# exactly the line EXPECTED, or, given QUIET instead, only if it exits 0 having written nothing, or,
# given EXPECTED_ERROR instead, only if it refuses its command line, having written exactly the
# lines EXPECTED_ERROR to stderr.
function(irama_add_cli_test name)
    cmake_parse_arguments(PARSE_ARGV 1 arg "QUIET" "EXPECTED;EXPECTED_ERROR" "ARGUMENTS")
    set(test_name "cli: ${name}")
    # Each separator written as a generator expression, so that add_test() keeps the list as one
    # argument, and the script gets it with plain separators.
    string(REPLACE ";" "$<SEMICOLON>" arguments "${arg_ARGUMENTS}")
    if(DEFINED arg_EXPECTED_ERROR)
        set(expected "-DEXPECTED_ERROR=${arg_EXPECTED_ERROR}")
    elseif(arg_QUIET)
        set(expected "-DQUIET=ON")
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

string(CONCAT usage
    "usage: irama-cli --version\n"
    "       irama-cli tone <file.wav> [--freq <hertz>] [--seconds <seconds>]\n"
    "       irama-cli synth <file.wav> --script <file> [--seconds <seconds>] [--rate <hertz>]"
)
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

# irama-cli synth plays a script through a player. Each refusal names the script's line.
set(scripts "${CMAKE_CURRENT_SOURCE_DIR}/scripts")
irama_add_cli_test("synth plays a chord script"
    ARGUMENTS synth "${CMAKE_CURRENT_BINARY_DIR}/chord.wav" --script "${scripts}/chord.txt"
    QUIET
)
irama_add_cli_test("synth plays a script that sets parameters"
    ARGUMENTS synth "${CMAKE_CURRENT_BINARY_DIR}/parameters.wav" --script "${scripts}/parameters.txt"
    QUIET
)
foreach(rate IN ITEMS 44100 48000 88200 96000)
    irama_add_cli_test("synth renders at ${rate} Hz"
        ARGUMENTS synth "${CMAKE_CURRENT_BINARY_DIR}/chord-${rate}.wav" --script "${scripts}/chord.txt"
            --seconds 0.1 --rate ${rate}
        QUIET
    )
endforeach()
foreach(rate IN ITEMS 22050 192000 48000.0 fast)
    irama_add_cli_test("synth refuses a rate of ${rate}"
        ARGUMENTS synth out.wav --script "${scripts}/chord.txt" --rate ${rate}
        EXPECTED_ERROR "irama-cli: --rate takes 44100, 48000, 88200 or 96000"
    )
endforeach()
irama_add_cli_test("synth without a script prints the usage"
    ARGUMENTS synth out.wav
    EXPECTED_ERROR "${usage}"
)
irama_add_cli_test("synth refuses a length of no seconds"
    ARGUMENTS synth out.wav --script "${scripts}/chord.txt" --seconds 0
    EXPECTED_ERROR "irama-cli: --seconds takes a number of seconds above 0, up to 3600"
)

# Adds a test that synth refuses the script `name` in scripts/, naming LINE and REASON.
function(irama_add_script_test description name line reason)
    irama_add_cli_test("synth refuses ${description}"
        ARGUMENTS synth out.wav --script "${scripts}/${name}.txt"
        EXPECTED_ERROR "irama-cli: ${scripts}/${name}.txt:${line}: ${reason}"
    )
endfunction()

irama_add_script_test("a malformed line" malformed 5 "expected `at <frame> <command>`")
irama_add_script_test("a word after a command" extra-word 4 "note-on takes a note and a velocity")
irama_add_script_test("a command it doesn't know" unknown-command 4
    "unknown command `pitch-bend`, expected note-on, note-off, release-all or set-parameter"
)
irama_add_script_test("a note out of range" note-out-of-range 4
    "note C-10 is out of range, C-0 to B-9"
)
irama_add_script_test("a note the note column doesn't write" not-a-note 4
    "`E#4` isn't a note, such as C-4 or F#3"
)
irama_add_script_test("a velocity out of range" velocity-out-of-range 5
    "velocity 1.5 is out of range, 0 to 1"
)
irama_add_script_test("a frame out of order" frame-out-of-order 5
    "frame 99 is before frame 100 on an earlier line"
)
irama_add_script_test("a parameter the instrument doesn't have" unknown-parameter 5
    "unknown parameter 4242"
)
irama_add_script_test("a parameter that isn't an ID" not-a-parameter 4
    "`gain` isn't a parameter ID, a whole number from 0"
)
irama_add_script_test("a parameter value that isn't a number" not-a-value 4
    "`loud` isn't a parameter value, a number"
)
irama_add_script_test("a set-parameter without a value" missing-value 4
    "set-parameter takes a parameter ID and a value"
)
