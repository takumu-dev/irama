# SPDX-FileCopyrightText: 2026 takumu-dev
# SPDX-License-Identifier: Zlib

# Runs PROGRAM with the arguments in ARGUMENTS, a list, and passes only if it exits 0 having written
# exactly the line EXPECTED to standard output.
execute_process(
    COMMAND "${PROGRAM}" ${ARGUMENTS}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)
if(NOT result STREQUAL "0")
    message(FATAL_ERROR "${PROGRAM} exited with ${result}:\n${error}")
endif()
if(NOT output STREQUAL "${EXPECTED}\n")
    message(FATAL_ERROR "${PROGRAM} wrote:\n${output}\nexpected:\n${EXPECTED}\n")
endif()
