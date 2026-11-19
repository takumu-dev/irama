# SPDX-FileCopyrightText: 2026 takumu-dev
# SPDX-License-Identifier: Zlib

# How libirama's components are declared, so that each one is shaped alike: a static library with
# its own public include tree, built without exceptions or RTTI.

include_guard(GLOBAL)

# Adds the component `name`, declared in libirama/src/<name>/, as the static library
# irama::<name>, and links it into irama::engine.
#
#   C_API        marks the C API, which sits on top of the components rather than among them: it
#                stays out of irama::engine, and keeps C++23 to itself, since its users may be C or
#                any C++ from C++11 on
#   HEADERS      its public headers, under include/irama/engine/<name>/ in its folder
#   SOURCES      its sources and private headers
#   DEPENDS      the other components it links, by name, each added before it
#   PACKAGES     the third-party targets it links privately
#   INCLUDE_DIR  the root of its public include tree, if not include/ in its folder
#
# A component includes only the headers of the components it links, since each public include tree
# comes only with its library.
function(irama_add_component name)
    cmake_parse_arguments(PARSE_ARGV 1 arg "C_API" "INCLUDE_DIR" "HEADERS;SOURCES;DEPENDS;PACKAGES")
    if(NOT DEFINED arg_INCLUDE_DIR)
        set(arg_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
    endif()
    set(target irama_${name})

    add_library(${target} STATIC)
    add_library(irama::${name} ALIAS ${target})
    target_sources(${target}
        PRIVATE ${arg_SOURCES}
        PUBLIC FILE_SET HEADERS BASE_DIRS "${arg_INCLUDE_DIR}" FILES ${arg_HEADERS}
    )
    if(arg_C_API)
        target_compile_features(${target} PRIVATE cxx_std_23)
    else()
        target_compile_features(${target} PUBLIC cxx_std_23)
    endif()
    foreach(dependency IN LISTS arg_DEPENDS)
        if(NOT TARGET irama::${dependency})
            message(FATAL_ERROR "The ${name} component links ${dependency}, which isn't added yet.")
        endif()
        target_link_libraries(${target} PUBLIC irama::${dependency})
    endforeach()
    target_link_libraries(${target} PRIVATE ${arg_PACKAGES})
    irama_target_warnings(${target})
    irama_target_no_exceptions(${target})

    if(NOT arg_C_API)
        target_link_libraries(irama_engine INTERFACE ${target})
    endif()
endfunction()
