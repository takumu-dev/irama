# SPDX-FileCopyrightText: 2026 takumu-dev
# SPDX-License-Identifier: Zlib

# The checks of the golden harness itself: cmake/golden_test.cmake and irama_wav_compare, run on
# `irama-cli tone` against goldens that irama_wav_fixture writes into the build tree, each wrong in
# one way. They are checks run through CMake, so they carry the `unit` label.

add_executable(irama_wav_fixture wav_fixture.cpp)
target_link_libraries(irama_wav_fixture PRIVATE irama::wav)
irama_target_conventions(irama_wav_fixture)

set(fixtures "${CMAKE_CURRENT_BINARY_DIR}/fixtures")
set(failures "${CMAKE_CURRENT_BINARY_DIR}/failures")

# Adds the setup test that writes the golden `name`.wav: `frames` frames of a sine wave at `hertz`,
# with `rate` frames a second and `channels` channels.
function(irama_add_fixture name rate channels frames hertz)
    set(test_name "golden harness: irama_wav_fixture writes the ${name} golden")
    add_test(NAME "${test_name}"
        COMMAND irama_wav_fixture "${fixtures}/${name}.wav" ${rate} ${channels} ${frames} ${hertz}
    )
    set_tests_properties("${test_name}" PROPERTIES LABELS unit FIXTURES_SETUP golden_harness)
endfunction()

file(MAKE_DIRECTORY "${fixtures}")
irama_add_fixture(matching 48000 2 48000 440)
irama_add_fixture(wrong-sample 48000 2 48000 441)
irama_add_fixture(wrong-rate 44100 2 48000 440)
irama_add_fixture(wrong-channels 48000 1 48000 440)
irama_add_fixture(wrong-length 48000 2 47999 440)
# What `irama-cli tone --freq 220 --seconds 0.5` writes.
irama_add_fixture(tone-220-hz-half-second 48000 2 24000 220)
# The update check replaces this golden, so it starts each run as it was written.
irama_add_fixture(replaced 48000 2 48000 441)
# 560,058 bytes, over 512 KiB.
irama_add_fixture(oversized 48000 2 70000 440)
# The update check creates this golden, so it starts each run missing.
add_test(NAME "golden harness: no golden created by an earlier run is left"
    COMMAND "${CMAKE_COMMAND}" -E rm -f "${fixtures}/created.wav"
)
set_tests_properties("golden harness: no golden created by an earlier run is left" PROPERTIES
    LABELS unit
    FIXTURES_SETUP golden_harness
)

# Adds the check `name`, which runs golden_test.cmake on irama-cli given ARGS, `tone` if none,
# with the fixture GOLDEN. With FAILS_WITH, it passes only if the script stops with a message
# matching it; without, only if the script passes, printing PRINTS if given. UPDATE sets
# IRAMA_UPDATE_GOLDENS, which is otherwise unset, whatever the caller's environment holds, and
# MAY_UPDATE lets the script write the golden.
function(irama_add_harness_check name)
    cmake_parse_arguments(PARSE_ARGV 1 arg "UPDATE;MAY_UPDATE" "GOLDEN;MAX_SIZE;FAILS_WITH;PRINTS"
        "ARGS"
    )
    if(NOT DEFINED arg_ARGS)
        set(arg_ARGS tone)
    endif()
    set(max_size "")
    if(DEFINED arg_MAX_SIZE)
        set(max_size MAX_SIZE ${arg_MAX_SIZE})
    endif()
    irama_add_golden_script_test("${name}"
        ARGS ${arg_ARGS}
        GOLDEN "${fixtures}/${arg_GOLDEN}.wav"
        ${max_size}
        MAY_UPDATE ${arg_MAY_UPDATE}
        FAILURES "${failures}"
        LABEL unit
    )
    if(arg_UPDATE)
        set(environment "IRAMA_UPDATE_GOLDENS=set:1")
    else()
        set(environment "IRAMA_UPDATE_GOLDENS=unset:")
    endif()
    set_tests_properties("${name}" PROPERTIES
        FIXTURES_REQUIRED golden_harness
        ENVIRONMENT_MODIFICATION "${environment}"
    )
    # CTest passes on the message alone, whatever the exit status, and only FATAL_ERROR prints it.
    if(DEFINED arg_FAILS_WITH)
        set_tests_properties("${name}" PROPERTIES
            PASS_REGULAR_EXPRESSION "CMake Error.*${arg_FAILS_WITH}"
        )
    else()
        set_tests_properties("${name}" PROPERTIES FAIL_REGULAR_EXPRESSION "CMake Error")
        if(DEFINED arg_PRINTS)
            set_tests_properties("${name}" PROPERTIES PASS_REGULAR_EXPRESSION "${arg_PRINTS}")
        endif()
    endif()
endfunction()

irama_add_harness_check("golden harness: a golden that matches within the tolerance passes" GOLDEN matching)
irama_add_harness_check("golden harness: a wrong sample fails, keeping the file irama-cli wrote"
    GOLDEN wrong-sample
    FAILS_WITH "largest error: .* differs from .*Kept the file irama-cli wrote at:.*/failures/wrong-sample\\.wav"
)
irama_add_harness_check("golden harness: a wrong rate fails" GOLDEN wrong-rate
    FAILS_WITH "has 48000 frames a second, but .* has 44100"
)
irama_add_harness_check("golden harness: a wrong channel count fails" GOLDEN wrong-channels
    FAILS_WITH "has 2 channels, but .* has 1"
)
irama_add_harness_check("golden harness: a wrong length fails" GOLDEN wrong-length
    FAILS_WITH "has 48000 frames, but .* has 47999"
)
irama_add_harness_check("golden harness: a golden over 512 KiB fails" GOLDEN oversized
    FAILS_WITH "is 560058 bytes, over the case's limit of 524288"
)
irama_add_harness_check("golden harness: a golden over its case's MAX_SIZE fails" GOLDEN matching MAX_SIZE 300000
    FAILS_WITH "is 384058 bytes, over the case's limit of 300000"
)
irama_add_harness_check("golden harness: a missing golden fails, naming the update command" GOLDEN missing
    FAILS_WITH "There is no golden at:.*Create it with:.*IRAMA_UPDATE_GOLDENS=1 ctest --preset gcc-release -L golden"
)
irama_add_harness_check("golden harness: updating creates a missing golden" GOLDEN created UPDATE MAY_UPDATE
    PRINTS "Created the golden:\n  .*/created\\.wav\n  peak: 0\\.5 at frame"
)
irama_add_harness_check("golden harness: updating reports the largest error from the old golden" GOLDEN replaced
    UPDATE MAY_UPDATE
    PRINTS "Updated the golden:\n  .*/replaced\\.wav\n  peak: 0\\.5 [^\n]*\n  largest error: "
)
irama_add_harness_check("golden harness: updating outside gcc-release fails" GOLDEN matching UPDATE
    FAILS_WITH "Only gcc-release writes goldens"
)

# irama-cli's options, checked against a file written apart from it.
irama_add_harness_check("cli: tone --freq and --seconds set the frequency and the length"
    ARGS tone --freq 220 --seconds 0.5
    GOLDEN tone-220-hz-half-second
)
