# SPDX-FileCopyrightText: 2026 takumu-dev
# SPDX-License-Identifier: Zlib

# The checks that end the program, each run through cmake/expect_abort.cmake.

# Adds the test `<component>: <name>`, which runs PROGRAM with ARGUMENTS through
# cmake/expect_abort.cmake: it passes if the program aborts after writing one line to stderr that
# matches REPORT, or, without REPORT, if the program exits 0 quietly.
function(irama_add_abort_test component name)
    cmake_parse_arguments(PARSE_ARGV 2 arg "" "PROGRAM;REPORT" "ARGUMENTS")
    set(test_name "${component}: ${name}")
    # Escaped, so the list reaches the script as one definition.
    string(REPLACE ";" "\\;" arguments "${arg_ARGUMENTS}")
    set(options "-DPROGRAM=$<TARGET_FILE:${arg_PROGRAM}>" "-DARGUMENTS=${arguments}")
    if(DEFINED arg_REPORT)
        list(APPEND options "-DREPORT=${arg_REPORT}")
    endif()
    add_test(NAME "${test_name}"
        COMMAND "${CMAKE_COMMAND}" ${options} -P "${PROJECT_SOURCE_DIR}/cmake/expect_abort.cmake"
    )
    set_tests_properties("${test_name}" PROPERTIES LABELS abort)
endfunction()

# The checks of irama/engine/base/assertion.hpp and what is built on it, compiled without NDEBUG
# and with it, whatever the build type.
foreach(variant IN ITEMS checked ndebug)
    set(target irama_abort_assertions_${variant})
    add_executable(${target} assertions.cpp)
    target_link_libraries(${target} PRIVATE irama::base)
    irama_target_conventions(${target})
endforeach()
target_compile_definitions(irama_abort_assertions_ndebug PRIVATE IRAMA_TEST_NDEBUG)

# The line a failed check in `file` writes, for the check `expression` with `message`.
function(irama_assertion_report var file expression message)
    foreach(literal IN ITEMS file expression message)
        string(REGEX REPLACE "([][()*+.?^$|\\])" "\\\\\\1" ${literal} "${${literal}}")
    endforeach()
    set(${var} "^.*${file}:[0-9]+: assertion `${expression}` failed: ${message}$" PARENT_SCOPE)
endfunction()

irama_assertion_report(square assertions.cpp "sides == 4" "a square has four sides")
irama_add_abort_test(base "a failed IRAMA_ASSERT aborts with its report"
    PROGRAM irama_abort_assertions_checked
    ARGUMENTS assert
    REPORT "${square}"
)
irama_add_abort_test(base "a failed IRAMA_ASSERT aborts with its report where NDEBUG is defined"
    PROGRAM irama_abort_assertions_ndebug
    ARGUMENTS assert
    REPORT "${square}"
)
irama_add_abort_test(base "a failed IRAMA_DEBUG_ASSERT aborts with its report"
    PROGRAM irama_abort_assertions_checked
    ARGUMENTS debug-assert
    REPORT "${square}"
)
irama_add_abort_test(base "IRAMA_DEBUG_ASSERT checks nothing where NDEBUG is defined"
    PROGRAM irama_abort_assertions_ndebug
    ARGUMENTS debug-assert
)

# Under RealtimeSanitizer, the report would otherwise be taken for a real-time violation.
irama_add_abort_test(base "a failed IRAMA_ASSERT in a nonblocking function aborts with its own report"
    PROGRAM irama_abort_assertions_checked
    ARGUMENTS nonblocking
    REPORT "${square}"
)

irama_assertion_report(narrowed assertions.cpp "std::in_range<To>(value)"
    "the value fits the type it is narrowed to"
)
irama_add_abort_test(base "narrowing a value too large for its type aborts with its report"
    PROGRAM irama_abort_assertions_checked
    ARGUMENTS narrow-too-large
    REPORT "${narrowed}"
)
irama_add_abort_test(base "narrowing a negative value to an unsigned type aborts with its report"
    PROGRAM irama_abort_assertions_checked
    ARGUMENTS narrow-negative
    REPORT "${narrowed}"
)
