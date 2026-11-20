# SPDX-FileCopyrightText: 2026 takumu-dev
# SPDX-License-Identifier: Zlib

# How libirama's components are declared, so that each one is shaped alike: a static library with
# its own public include tree, built without exceptions or RTTI; and the check that keeps what they
# link to a fixed list.

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

# Checks, once the whole project is configured, that each target declared in the calling directory
# or below it, libirama's components among them, links nothing but other such targets and the
# imported targets in ARGN. Only a fixed list is allowed, rather than each component's PACKAGES, so
# that no component can admit a package by declaring it. A link is read as written, so one inside a
# generator expression fails the check rather than hide what it links. The check stops the
# configuration, naming each link it rejects.
function(irama_check_link_closure)
    # A deferred call reads its arguments only when it runs, so they're written into it now.
    cmake_language(EVAL CODE "
        cmake_language(DEFER DIRECTORY [[${CMAKE_SOURCE_DIR}]]
            CALL irama_check_links [[${CMAKE_CURRENT_SOURCE_DIR}]] [[${ARGN}]]
        )
    ")
endfunction()

# The deferred half of irama_check_link_closure(): checks the links of the targets declared in
# `directory` or below it against the imported targets in the list `allowed`.
function(irama_check_links directory allowed)
    set(targets "")
    set(directories "${directory}")
    while(directories)
        list(POP_FRONT directories current)
        get_property(current_targets DIRECTORY "${current}" PROPERTY BUILDSYSTEM_TARGETS)
        get_property(subdirectories DIRECTORY "${current}" PROPERTY SUBDIRECTORIES)
        list(APPEND targets ${current_targets})
        list(APPEND directories ${subdirectories})
    endwhile()

    set(problems "")
    foreach(target IN LISTS targets)
        set(links "")
        foreach(property IN ITEMS LINK_LIBRARIES INTERFACE_LINK_LIBRARIES)
            get_target_property(property_links ${target} ${property})
            if(property_links)
                list(APPEND links ${property_links})
            endif()
        endforeach()
        foreach(link IN LISTS links)
            # A static library passes on its private links as $<LINK_ONLY:…>, and CMake sets off a
            # link made from another directory between markers that begin with ::@.
            if(link MATCHES "^\\$<LINK_ONLY:(.*)>$")
                set(link "${CMAKE_MATCH_1}")
            endif()
            if(link MATCHES "^::@" OR link IN_LIST allowed)
                continue()
            endif()
            if(link MATCHES "\\$<")
                list(APPEND problems
                    "${target} links ${link}, a generator expression, which the check can't follow."
                )
                continue()
            endif()
            if(TARGET "${link}")
                get_target_property(aliased "${link}" ALIASED_TARGET)
                if(aliased IN_LIST targets OR link IN_LIST targets)
                    continue()
                endif()
            endif()
            list(APPEND problems "${target} links ${link}.")
        endforeach()
    endforeach()

    if(problems)
        list(REMOVE_DUPLICATES problems)
        list(SORT problems)
        list(TRANSFORM problems PREPEND "  - ")
        list(JOIN problems "\n" problems)
        string(CONCAT remedy "A libirama target may link only other libirama targets and the "
            "imported targets that libirama/CMakeLists.txt allows."
        )
        message(FATAL_ERROR "libirama links what it may not:\n${problems}\n${remedy}")
    endif()
endfunction()
