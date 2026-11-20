# SPDX-FileCopyrightText: 2026 takumu-dev
# SPDX-License-Identifier: Zlib

# Runs a program that must abort, and checks how it ends: a Catch2 test case can't watch its own
# process abort, and CTest counts an abort as a failure even in a test expected to fail.
#
#   PROGRAM    the program to run
#   ARGUMENTS  its arguments, as a list
#   REPORT     a regular expression that the one line the program writes to stderr must match
#              before it aborts. Without REPORT, the program must instead exit 0 and write nothing
#              to stderr, as it does where the check it makes is compiled out.
#
# A sanitizer's report runs to several lines and ends the program with a status of its own, so a
# sanitizer that reports in place of the program fails the test.

execute_process(
    COMMAND "${PROGRAM}" ${ARGUMENTS}
    RESULT_VARIABLE result
    ERROR_VARIABLE stderr
)
string(REPLACE "\r\n" "\n" stderr "${stderr}")

if(NOT DEFINED REPORT)
    if(NOT result STREQUAL "0" OR NOT stderr STREQUAL "")
        message(FATAL_ERROR "${PROGRAM} should have exited 0 quietly, but it ended with "
            "${result}, writing to stderr:\n${stderr}"
        )
    endif()
    return()
endif()

# POSIX reports the signal. On Windows, abort() exits with status 3.
if(NOT (result MATCHES "aborted" OR (CMAKE_HOST_WIN32 AND result STREQUAL "3")))
    message(FATAL_ERROR "${PROGRAM} should have aborted, but it ended with ${result}, writing to "
        "stderr:\n${stderr}"
    )
endif()
string(REGEX MATCHALL "\n" newlines "${stderr}")
list(LENGTH newlines lines)
string(REGEX REPLACE "\n$" "" report "${stderr}")
if(NOT lines EQUAL 1 OR NOT stderr MATCHES "\n$" OR NOT report MATCHES "${REPORT}")
    message(FATAL_ERROR "${PROGRAM} should have written one line to stderr matching:\n${REPORT}\n"
        "but it wrote:\n${stderr}"
    )
endif()
