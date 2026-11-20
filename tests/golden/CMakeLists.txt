# SPDX-FileCopyrightText: 2026 takumu-dev
# SPDX-License-Identifier: Zlib

# The golden cases: irama-cli writes a WAV file, which cmake/golden_test.cmake compares with the
# case's golden, tests/golden/<case>.wav, sample by sample.

# dr_wav is a single header, with no CMake package of its own.
find_path(IRAMA_DR_WAV_INCLUDE_DIR dr_wav.h REQUIRED)
add_executable(irama_wav_compare wav_compare.cpp)
target_include_directories(irama_wav_compare SYSTEM PRIVATE "${IRAMA_DR_WAV_INCLUDE_DIR}")
target_link_libraries(irama_wav_compare PRIVATE irama::base)
irama_target_conventions(irama_wav_compare)
# dr_wav opens files with std::fopen, which MSVC deprecates in favour of its own fopen_s.
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_definitions(irama_wav_compare PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# Goldens are written only from the gcc-release preset's build tree, build/gcc-release, so that
# they don't drift between compilers.
cmake_path(GET PROJECT_BINARY_DIR FILENAME irama_build_tree)
string(COMPARE EQUAL "${irama_build_tree}" "gcc-release" irama_may_update_goldens)

# Adds the test `name`, labelled LABEL, which runs cmake/golden_test.cmake with GOLDEN, MAX_SIZE,
# MAY_UPDATE and FAILURES as its settings of the same names, and TOLERANCE as its MAX_ERROR. Each
# sample may differ from its golden by 1e-5 unless TOLERANCE says otherwise, and the golden may be
# 512 KiB, about 1.3 seconds, unless MAX_SIZE says otherwise. irama-cli is given ARGS, then the path
# of the file to write.
function(irama_add_golden_script_test name)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "GOLDEN;TOLERANCE;MAX_SIZE;MAY_UPDATE;FAILURES;LABEL"
        "ARGS"
    )
    if(NOT DEFINED arg_TOLERANCE)
        set(arg_TOLERANCE 1e-5)
    endif()
    if(NOT DEFINED arg_MAX_SIZE)
        math(EXPR arg_MAX_SIZE "512 * 1024")
    endif()
    # Each separator written as a generator expression, so that add_test() keeps the list as one
    # argument, and the script gets it with plain separators.
    string(REPLACE ";" "$<SEMICOLON>" arguments "${arg_ARGS}")
    # Named after the test, so that tests sharing a golden don't share the file irama-cli writes.
    string(MAKE_C_IDENTIFIER "${name}" actual)
    add_test(NAME "${name}"
        COMMAND "${CMAKE_COMMAND}"
            "-DCLI=$<TARGET_FILE:irama-cli>"
            "-DARGUMENTS=${arguments}"
            "-DCOMPARE=$<TARGET_FILE:irama_wav_compare>"
            "-DGOLDEN=${arg_GOLDEN}"
            "-DACTUAL=${CMAKE_CURRENT_BINARY_DIR}/${actual}.wav"
            "-DMAX_ERROR=${arg_TOLERANCE}"
            "-DMAX_SIZE=${arg_MAX_SIZE}"
            "-DFAILURES=${arg_FAILURES}"
            "-DMAY_UPDATE=${arg_MAY_UPDATE}"
            -P "${PROJECT_SOURCE_DIR}/cmake/golden_test.cmake"
    )
    set_tests_properties("${name}" PROPERTIES LABELS ${arg_LABEL})
endfunction()

# Adds the golden case `golden: <name>`, whose golden is tests/golden/<name>.wav. irama-cli is
# given ARGS, then the path of the file to write. Each sample may differ from its golden by 1e-5,
# or by TOLERANCE, and the golden may be 512 KiB, about 1.3 seconds, or MAX_SIZE bytes. A TOLERANCE
# or MAX_SIZE comes with a comment saying why.
function(irama_add_golden_test name)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "TOLERANCE;MAX_SIZE" "ARGS")
    set(limits "")
    foreach(limit IN ITEMS TOLERANCE MAX_SIZE)
        if(DEFINED arg_${limit})
            list(APPEND limits ${limit} ${arg_${limit}})
        endif()
    endforeach()
    irama_add_golden_script_test("golden: ${name}"
        ARGS ${arg_ARGS}
        GOLDEN "${CMAKE_CURRENT_SOURCE_DIR}/${name}.wav"
        ${limits}
        MAY_UPDATE ${irama_may_update_goldens}
        FAILURES "${PROJECT_BINARY_DIR}/golden-failures"
        LABEL golden
    )
endfunction()

irama_add_golden_test(tone ARGS tone)

add_subdirectory(harness)
