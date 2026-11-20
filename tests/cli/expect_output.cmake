# SPDX-FileCopyrightText: 2026 takumu-dev
# SPDX-License-Identifier: Zlib

# Runs PROGRAM with the arguments in ARGUMENTS, a list, and passes only if it exits 0 having written
# exactly the line EXPECTED to standard output, or, given EXPECTED_ERROR instead, only if it exits
# 2, refusing its command line, having written exactly the lines EXPECTED_ERROR to stderr.
execute_process(
    COMMAND "${PROGRAM}" ${ARGUMENTS}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)
if(DEFINED EXPECTED_ERROR)
    string(REPLACE "\r\n" "\n" error "${error}")
    if(NOT result STREQUAL "2" OR NOT error STREQUAL "${EXPECTED_ERROR}\n")
        message(FATAL_ERROR "${PROGRAM} exited with ${result}, writing to stderr:\n${error}\n"
            "expected it to exit with 2, writing:\n${EXPECTED_ERROR}\n"
        )
    endif()
    return()
endif()
if(NOT result STREQUAL "0")
    message(FATAL_ERROR "${PROGRAM} exited with ${result}:\n${error}")
endif()
if(NOT output STREQUAL "${EXPECTED}\n")
    message(FATAL_ERROR "${PROGRAM} wrote:\n${output}\nexpected:\n${EXPECTED}\n")
endif()
