# SPDX-FileCopyrightText: 2026 takumu-dev
# SPDX-License-Identifier: Zlib

# The language and warning settings every target of libirama, irama-cli and the tests builds with.
#
# Everything here is PRIVATE to the target. Warnings become errors only through
# CMAKE_COMPILE_WARNING_AS_ERROR, which Irama's own presets set; a game building libirama as a
# subproject keeps its own choice.

include_guard(GLOBAL)

# Standard C++23 as ordinary headers: no compiler extensions, and no scanning for C++ modules, which
# Irama doesn't use. The C sources are standard C as well.
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_C_EXTENSIONS OFF)

# Turns on the strict warnings, and the strict conformance mode on MSVC. Third-party headers come in
# as SYSTEM includes, so these apply to Irama's own code alone. The flags that only make sense for
# C++ are kept off a target's C sources.
function(irama_target_warnings target)
    set(cxx "$<COMPILE_LANGUAGE:CXX>")
    if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        target_compile_options(${target} PRIVATE
            /W4
            /permissive-
            $<${cxx}:/Zc:__cplusplus>
        )
    else()
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wsign-conversion
            -Wshadow
            -Wcast-align
            -Wnull-dereference
            -Wdouble-promotion
            -Wformat=2
            -Wimplicit-fallthrough
            $<${cxx}:-Wold-style-cast>
            $<${cxx}:-Wnon-virtual-dtor>
            $<${cxx}:-Woverloaded-virtual>
        )
    endif()
endfunction()

# Builds the target's own sources without exceptions or RTTI, as libirama's are in every build, so
# a stray `throw` or `dynamic_cast` fails everywhere. irama-cli and the tests keep the compiler's
# defaults.
function(irama_target_no_exceptions target)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        target_compile_options(${target} PRIVATE /EHs-c- /GR-)
        target_compile_definitions(${target} PRIVATE _HAS_EXCEPTIONS=0)
    else()
        target_compile_options(${target} PRIVATE -fno-exceptions -fno-rtti)
    endif()
endfunction()

# Applies the settings every C++ target of Irama shares: C++23 and the strict warnings.
function(irama_target_conventions target)
    target_compile_features(${target} PRIVATE cxx_std_23)
    irama_target_warnings(${target})
endfunction()
