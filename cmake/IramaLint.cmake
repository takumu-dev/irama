# SPDX-FileCopyrightText: 2026 takumu-dev
# SPDX-License-Identifier: Zlib

# The checks of the code that sit beside the build: formatting, clang-tidy and the documentation.
# Each LLVM tool is found by its versioned package name first, such as `clang-format-23`, then by
# its plain name, and a tool older than its floor in IramaFloors.cmake stops the configuration. Each
# target exists only where its tool is found, so the lint preset fails at once, naming the target,
# on a machine that lacks one.
#
#   check-format  fails if any source differs from what clang-format would make of it
#   format        reformats every source in place
#   docs          runs Doxygen over the public headers into build/<preset>/docs, failing on any
#                 warning, an undocumented declaration among them
#
# With IRAMA_CLANG_TIDY on, clang-tidy checks every C++ source of every target as it compiles, so
# this module must be included before any target is added, and the target tidy-irama-h checks
# irama.h, which the build makes part of `all`.

include_guard(GLOBAL)

option(IRAMA_CLANG_TIDY "Run clang-tidy on every C++ source as it compiles." OFF)

# Finds the LLVM tool `name` into the cache variable `var`, and stops the configuration if it is
# older than its floor. `var` stays unset if there is no such tool.
function(irama_find_llvm_tool var name)
    find_program(${var} NAMES ${name}-${irama_clang_floor} ${name})
    if(NOT ${var})
        return()
    endif()
    # The version follows "version", which distributions may wrap in a vendor's name and a build
    # string, as in "Ubuntu clang-format version 23.1.1 (1~exp1)".
    execute_process(
        COMMAND "${${var}}" --version
        OUTPUT_VARIABLE output
        ERROR_VARIABLE output
        RESULT_VARIABLE result
    )
    if(NOT result STREQUAL "0" OR NOT output MATCHES "version ([0-9]+(\\.[0-9]+)*)")
        message(FATAL_ERROR "Couldn't read the version of ${${var}}:\n${output}")
    endif()
    irama_check_tool_floor(${name} "${CMAKE_MATCH_1}")
endfunction()

irama_find_llvm_tool(IRAMA_CLANG_FORMAT clang-format)
if(IRAMA_CLANG_FORMAT)
    block()
        set(sources "")
        foreach(part IN ITEMS cli libirama tests)
            file(GLOB_RECURSE part_sources CONFIGURE_DEPENDS
                "${PROJECT_SOURCE_DIR}/${part}/*.c"
                "${PROJECT_SOURCE_DIR}/${part}/*.h"
                "${PROJECT_SOURCE_DIR}/${part}/*.cpp"
                "${PROJECT_SOURCE_DIR}/${part}/*.hpp"
            )
            list(APPEND sources ${part_sources})
        endforeach()
        add_custom_target(check-format
            COMMAND "${IRAMA_CLANG_FORMAT}" --dry-run --Werror ${sources}
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
            COMMENT "Checking the formatting"
            VERBATIM
        )
        add_custom_target(format
            COMMAND "${IRAMA_CLANG_FORMAT}" -i ${sources}
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
            COMMENT "Formatting the sources"
            VERBATIM
        )
    endblock()
endif()

# A preset can name only the plain clang-tidy, so the switch finds the tool, and hands it to every
# target through CMAKE_CXX_CLANG_TIDY. The C sources aren't checked: there are too few of them to
# be worth a configuration of their own.
if(IRAMA_CLANG_TIDY)
    irama_find_llvm_tool(IRAMA_CLANG_TIDY_PROGRAM clang-tidy)
    if(NOT IRAMA_CLANG_TIDY_PROGRAM)
        message(FATAL_ERROR "IRAMA_CLANG_TIDY is on, but there is no clang-tidy on the PATH.")
    endif()
    set(CMAKE_CXX_CLANG_TIDY "${IRAMA_CLANG_TIDY_PROGRAM}")

    # clang-tidy reads the configuration beside the file it is run on, never beside a header that
    # file includes, so the C++ sources skip irama.h (see .clang-tidy). It is checked here on its
    # own, as the oldest C++ it compiles as, under the C rules in the .clang-tidy beside it, and
    # again whenever it or either configuration changes.
    set(irama_h "${PROJECT_SOURCE_DIR}/libirama/include/irama/irama.h")
    add_custom_command(
        OUTPUT "${PROJECT_BINARY_DIR}/tidy-irama-h.stamp"
        COMMAND "${IRAMA_CLANG_TIDY_PROGRAM}" --quiet "${irama_h}" -- -x c++ -std=c++11
        COMMAND "${CMAKE_COMMAND}" -E touch "${PROJECT_BINARY_DIR}/tidy-irama-h.stamp"
        DEPENDS
            "${irama_h}"
            "${PROJECT_SOURCE_DIR}/libirama/include/irama/.clang-tidy"
            "${PROJECT_SOURCE_DIR}/.clang-tidy"
        COMMENT "Checking irama.h with clang-tidy"
        VERBATIM
    )
    add_custom_target(tidy-irama-h ALL DEPENDS "${PROJECT_BINARY_DIR}/tidy-irama-h.stamp")
    unset(irama_h)
endif()

find_package(Doxygen QUIET)
if(TARGET Doxygen::doxygen)
    irama_check_tool_floor(Doxygen "${DOXYGEN_VERSION}")
    add_custom_target(docs
        COMMAND "${CMAKE_COMMAND}" -E env "IRAMA_DOCS_DIR=${PROJECT_BINARY_DIR}/docs"
            "$<TARGET_FILE:Doxygen::doxygen>" Doxyfile
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        COMMENT "Checking the documentation of the public headers"
        VERBATIM
    )
endif()
