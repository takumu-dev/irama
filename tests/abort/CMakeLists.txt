# SPDX-FileCopyrightText: 2026 takumu-dev
# SPDX-License-Identifier: Zlib

# The checks that end the program, each run through cmake/expect_abort.cmake.

# Adds the test `<component>: <name>`, which runs PROGRAM with ARGUMENTS through
# cmake/expect_abort.cmake: it passes if the program aborts after writing one line to stderr that
# matches REPORT, if RealtimeSanitizer stops it for calling REALTIME_SANITIZER_CALL, or, without
# either, if the program exits 0 quietly.
function(irama_add_abort_test component name)
    cmake_parse_arguments(PARSE_ARGV 2 arg "" "PROGRAM;REPORT;REALTIME_SANITIZER_CALL" "ARGUMENTS")
    set(test_name "${component}: ${name}")
    # Escaped, so the list reaches the script as one definition.
    string(REPLACE ";" "\\;" arguments "${arg_ARGUMENTS}")
    set(options "-DPROGRAM=$<TARGET_FILE:${arg_PROGRAM}>" "-DARGUMENTS=${arguments}")
    if(DEFINED arg_REPORT)
        list(APPEND options "-DREPORT=${arg_REPORT}")
    endif()
    if(DEFINED arg_REALTIME_SANITIZER_CALL)
        list(APPEND options "-DREALTIME_SANITIZER_CALL=${arg_REALTIME_SANITIZER_CALL}")
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

# The checks of irama/engine/base/render_scope.hpp, made by the real-time guard where it is compiled
# in, and by RealtimeSanitizer in its place where that is on.
find_package(Threads REQUIRED)
add_executable(irama_abort_render_scope render_scope.cpp)
target_link_libraries(irama_abort_render_scope PRIVATE irama::base Threads::Threads)
irama_target_conventions(irama_abort_render_scope)

include(CheckCXXSourceCompiles)
check_cxx_source_compiles([[
    #ifdef __has_feature
    #if __has_feature(realtime_sanitizer)
    #define IRAMA_REALTIME_SANITIZER
    #endif
    #endif
    #ifndef IRAMA_REALTIME_SANITIZER
    #error RealtimeSanitizer is off.
    #endif
    int main() {}
]] irama_realtime_sanitizer)

# Adds the test of the case `name` of the program, which breaks the real-time rules as `what`
# says: `action` is `allocated` or `freed`, and `call` the C library function that does it. The
# test expects the guard's report where the guard is compiled in, RealtimeSanitizer's where that is
# on instead, and a quiet exit 0 where neither is.
function(irama_add_render_scope_test name what action call)
    if(IRAMA_REALTIME_GUARD)
        set(ending "aborts with the guard's report")
        set(expected REPORT "^real-time guard: ${action} [0-9]+ bytes inside a render call$")
    elseif(irama_realtime_sanitizer)
        set(ending "stops with RealtimeSanitizer's report")
        set(expected REALTIME_SANITIZER_CALL "${call}")
    else()
        set(ending "goes unchecked where neither check is compiled in")
        set(expected "")
    endif()
    irama_add_abort_test(base "${what} ${ending}"
        PROGRAM irama_abort_render_scope
        ARGUMENTS ${name}
        ${expected}
    )
endfunction()

irama_add_render_scope_test(new-inside "new inside a render scope" allocated malloc)
irama_add_render_scope_test(delete-inside "delete inside a render scope" freed free)
# libstdc++'s aligned operator new calls posix_memalign.
irama_add_render_scope_test(aligned-new-inside "aligned new inside a render scope" allocated
    posix_memalign
)
irama_add_render_scope_test(aligned-delete-inside "aligned delete inside a render scope" freed free)
irama_add_render_scope_test(new-after-nested "new after a nested render scope ends" allocated malloc)

irama_add_abort_test(base "new and aligned new outside any render scope are allowed"
    PROGRAM irama_abort_render_scope
    ARGUMENTS new-outside
)
irama_add_abort_test(base "new while another thread renders is allowed"
    PROGRAM irama_abort_render_scope
    ARGUMENTS new-beside
)
