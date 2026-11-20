# SPDX-FileCopyrightText: 2026 takumu-dev
# SPDX-License-Identifier: Zlib

# The oldest compilers, standard library and checking tools Irama supports. The floors never fall,
# and configuring with anything older stops here, rather than somewhere deep in a standard header or
# in a check that disagrees with CI's. The CMake floor is cmake_minimum_required's, at the top of
# the project.

include_guard(GLOBAL)

# Clang's floor, which LLVM's clang-format and clang-tidy follow, since each release formats and
# checks differently.
set(irama_clang_floor 23)

# Stops the configuration if the C++ compiler with this CMake compiler id and version is one Irama
# supports, but older than its floor. Any other compiler gets a warning.
function(irama_check_compiler_floor id version)
    if(id STREQUAL "GNU")
        set(floor 16)
        set(needed "GCC 16")
        set(found "GCC ${version}")
    elseif(id STREQUAL "Clang")
        set(floor ${irama_clang_floor})
        set(needed "Clang ${irama_clang_floor}")
        set(found "Clang ${version}")
    elseif(id STREQUAL "MSVC")
        # CMake reports cl's own version, which runs five majors ahead of the toolset's.
        set(floor 19.52)
        set(needed "MSVC 14.52 (cl 19.52)")
        set(found "cl ${version}")
    else()
        # Nothing is known about other compilers, so they may try.
        message(WARNING
            "Irama supports GCC, Clang and MSVC; ${id} ${version} is untested, and the build may "
            "fail."
        )
        return()
    endif()

    if(version VERSION_LESS floor)
        message(FATAL_ERROR "Irama needs ${needed} or newer; this is ${found}.")
    endif()
endfunction()

# Stops the configuration if `release`, the value of _GLIBCXX_RELEASE, is below libstdc++'s floor.
# An empty `release` means another standard library, which has no floor here.
function(irama_check_libstdcxx_floor release)
    if(release STREQUAL "")
        return()
    endif()
    if(release LESS 16)
        message(FATAL_ERROR "Irama needs libstdc++ 16 or newer; this is libstdc++ ${release}.")
    endif()
endfunction()

# Sets `out_var` to the _GLIBCXX_RELEASE of the standard library the C++ compiler uses, or to an
# empty string if it uses another. Clang takes whichever libstdc++ the system has, so the compiler
# floor alone doesn't settle it.
function(irama_detect_libstdcxx_release out_var)
    # The release is spelt into a string the compiled file keeps, which CMake's own compiler
    # detection reads the same way, so nothing has to run.
    set(probe [=[
#include <version>
#define IRAMA_STRINGIFY_(x) #x
#define IRAMA_STRINGIFY(x) IRAMA_STRINGIFY_(x)
#ifdef _GLIBCXX_RELEASE
const char* info = "INFO:libstdcxx[" IRAMA_STRINGIFY(_GLIBCXX_RELEASE) "]";
#else
const char* info = "INFO:libstdcxx[]";
#endif
int main(int argc, char**) { return info[argc]; }
]=])
    set(probe_binary "${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/irama_libstdcxx_probe")
    try_compile(compiled
        SOURCE_FROM_CONTENT libstdcxx_probe.cpp "${probe}"
        CXX_STANDARD 23
        COPY_FILE "${probe_binary}"
        OUTPUT_VARIABLE output
    )
    if(NOT compiled)
        message(FATAL_ERROR "Couldn't compile a check of the C++ standard library:\n${output}")
    endif()
    file(STRINGS "${probe_binary}" line REGEX "INFO:libstdcxx\\[[0-9]*\\]")
    string(REGEX MATCH "INFO:libstdcxx\\[([0-9]*)\\]" _ "${line}")
    set(${out_var} "${CMAKE_MATCH_1}" PARENT_SCOPE)
endfunction()

# Stops the configuration if `tool`, one of the checking tools clang-format, clang-tidy and Doxygen,
# is at a `version` older than its floor.
function(irama_check_tool_floor tool version)
    if(tool STREQUAL "clang-format" OR tool STREQUAL "clang-tidy")
        set(floor ${irama_clang_floor})
    elseif(tool STREQUAL "Doxygen")
        set(floor 1.18)
    else()
        message(FATAL_ERROR "Irama has no floor for the tool ${tool}.")
    endif()

    if(version VERSION_LESS floor)
        message(FATAL_ERROR "Irama needs ${tool} ${floor} or newer; this is ${tool} ${version}.")
    endif()
endfunction()
