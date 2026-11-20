# SPDX-FileCopyrightText: 2026 takumu-dev
# SPDX-License-Identifier: Zlib

# Checks the licence of every port in a vcpkg installed tree against Irama's two allowlists, and
# checks or writes the third-party notices of the applications.
#
#   cmake -D INSTALLED_DIR=<dir> -D MANIFEST=<file> -D TRIPLET=<triplet> [-D LICENSES=<file>]
#         [-D APPLICATIONS=<feature>=<application>,...
#          (-D CHECK_NOTICES=<file> | -D WRITE_NOTICES=<file>)]
#         -P cmake/check_licenses.cmake
#
#   INSTALLED_DIR  the vcpkg_installed directory, holding every triplet's ports
#   MANIFEST       Irama's vcpkg.json
#   TRIPLET        the triplet libirama and the applications are built for
#   LICENSES       the licences file; cmake/licenses.json beside this script by default
#   APPLICATIONS   the applications that ship, separated by commas, each the manifest feature that
#                  holds its dependencies and the name the notices give it
#   CHECK_NOTICES  a notices file that must match what the applications ship
#   WRITE_NOTICES  a notices file to write for what the applications ship
#
# Each port's licence is the SPDX expression in its share/<port>/vcpkg.spdx.json. The licences file
# holds two allowlists, each of SPDX licences or `<licence> WITH <exception>`:
#
#   libirama      for libirama's ports: those that the manifest's top-level dependencies and its
#                 `examples` feature name, except build tools, and every port they depend on for
#                 TRIPLET, since the examples are copied into games and owe what libirama owes
#   applications  for every other port, build tools and the tests' dependencies included
#
# `fonts` adds its `licenses` to the applications' list for the ports it names in its `ports`, and
# for no others. `overrides` each give a port's licence as a review found it, with the `port`, the
# `version` reviewed as vcpkg prints it, the `license` and the `reason`. Once the port's version
# changes, its override fails the check until it is reviewed again. The licence an override gives
# must still be on the port's list, so an override can correct what a port declares but never admit
# a licence the list lacks. The check fails, naming every port it rejects.
#
# What an application ships is every port its feature's dependencies name, except build tools, and
# every port they depend on for TRIPLET. Only those are listed in the notices, each with the
# applications that use it: libirama's ports need no notice, since its allowlist admits only
# licences that ask for none.
cmake_minimum_required(VERSION 4.4)

# Stops with the heading, the problems in ARGN beneath it, sorted, and then the remedy, if any. Each
# problem is indented, so CMake prints it on one line rather than wrapping it, and sets it apart.
function(irama_fail heading remedy)
    set(problems ${ARGN})
    list(SORT problems)
    list(TRANSFORM problems PREPEND "  - ")
    list(JOIN problems "\n" problems)
    message(FATAL_ERROR "${heading}\n${problems}\n${remedy}")
endfunction()

# Sets `out` to the strings in the JSON array at the path in ARGN, or to nothing if there is none.
function(irama_json_strings out json)
    set(strings "")
    string(JSON count ERROR_VARIABLE error LENGTH "${json}" ${ARGN})
    if(NOT error AND count GREATER 0)
        math(EXPR last "${count} - 1")
        foreach(index RANGE ${last})
            string(JSON string GET "${json}" ${ARGN} ${index})
            list(APPEND strings "${string}")
        endforeach()
    endif()
    set(${out} "${strings}" PARENT_SCOPE)
endfunction()

# Sets `out` to 1 if the licences in ARGN cover the SPDX licence expression, and to 0 otherwise. An
# AND needs every side covered and an OR any one, with AND binding tighter. A licence with an
# exception, `<licence> WITH <exception>`, is covered only if ARGN names it so. An expression that
# can't be read isn't covered.
function(irama_license_allowed out expression)
    set(allowed ${ARGN})
    string(REPLACE "(" " ( " expression "${expression}")
    string(REPLACE ")" " ) " expression "${expression}")
    string(STRIP "${expression}" expression)
    string(REGEX REPLACE "[ \t\n]+" ";" tokens "${expression}")

    # With each licence, and its exception, replaced by whether it is allowed, only AND, OR and
    # parentheses are left to evaluate.
    set(values "")
    list(LENGTH tokens count)
    set(index 0)
    while(index LESS count)
        list(GET tokens ${index} token)
        math(EXPR next "${index} + 1")
        if(token MATCHES "^(AND|OR|\\(|\\))$")
            list(APPEND values "${token}")
        elseif(token STREQUAL "WITH")
            set(${out} 0 PARENT_SCOPE)
            return()
        else()
            set(license "${token}")
            if(next LESS count)
                list(GET tokens ${next} following)
                if(following STREQUAL "WITH")
                    math(EXPR exception "${next} + 1")
                    if(exception GREATER_EQUAL count)
                        set(${out} 0 PARENT_SCOPE)
                        return()
                    endif()
                    list(GET tokens ${exception} exception)
                    string(APPEND license " WITH ${exception}")
                    math(EXPR next "${next} + 2")
                endif()
            endif()
            if(license IN_LIST allowed)
                list(APPEND values 1)
            else()
                list(APPEND values 0)
            endif()
        endif()
        set(index ${next})
    endwhile()
    list(JOIN values " " values)

    # CMake's regular expressions can't match nested parentheses, so the innermost groups go first.
    while(values MATCHES "\\(([^()]*)\\)")
        set(group "${CMAKE_MATCH_0}")
        irama_flat_license_value(value "${CMAKE_MATCH_1}")
        if(value STREQUAL "")
            set(${out} 0 PARENT_SCOPE)
            return()
        endif()
        string(REPLACE "${group}" "${value}" values "${values}")
    endwhile()
    irama_flat_license_value(value "${values}")
    if(value STREQUAL "")
        set(value 0)
    endif()
    set(${out} ${value} PARENT_SCOPE)
endfunction()

# Sets `out` to the value of a run of 1s and 0s joined by AND and OR, with no parentheses, or to
# nothing if the run doesn't read as one.
function(irama_flat_license_value out values)
    string(STRIP "${values}" values)
    if(NOT values MATCHES "^[01]( (AND|OR) [01])*$")
        set(${out} "" PARENT_SCOPE)
        return()
    endif()
    string(REPLACE " OR " ";" alternatives "${values}")
    foreach(alternative IN LISTS alternatives)
        if(NOT alternative MATCHES "0")
            set(${out} 1 PARENT_SCOPE)
            return()
        endif()
    endforeach()
    set(${out} 0 PARENT_SCOPE)
endfunction()

# Appends to the list `out` the names in the manifest's array of dependencies at the path in ARGN,
# where each is a name or an object with one, except dependencies on build tools, which are marked
# `host`.
function(irama_manifest_names out manifest)
    set(names "${${out}}")
    string(JSON count ERROR_VARIABLE error LENGTH "${manifest}" ${ARGN})
    if(error OR count EQUAL 0)
        return()
    endif()
    math(EXPR last "${count} - 1")
    foreach(index RANGE ${last})
        string(JSON type TYPE "${manifest}" ${ARGN} ${index})
        if(type STREQUAL "STRING")
            string(JSON name GET "${manifest}" ${ARGN} ${index})
            list(APPEND names "${name}")
        else()
            string(JSON name GET "${manifest}" ${ARGN} ${index} name)
            string(JSON host ERROR_VARIABLE error GET "${manifest}" ${ARGN} ${index} host)
            if(NOT host)
                list(APPEND names "${name}")
            endif()
        endif()
    endforeach()
    set(${out} "${names}" PARENT_SCOPE)
endfunction()

# Sets `out` to the installed ports, as <name>/<triplet>, that the ports named in ARGN are for
# TRIPLET, together with every port they depend on for TRIPLET, read from depends_<name>/<triplet>.
# A name in ARGN that isn't installed for TRIPLET is left out. vcpkg's own helper ports, named
# vcpkg-*, only run while ports build. Where the build tools share TRIPLET, vcpkg lists them as
# ordinary dependencies, so they are left out by name.
function(irama_port_closure out)
    set(queue "")
    foreach(name IN LISTS ARGN)
        if("${name}/${TRIPLET}" IN_LIST installed)
            list(APPEND queue "${name}/${TRIPLET}")
        endif()
    endforeach()
    set(closure "")
    while(queue)
        list(POP_FRONT queue package)
        if(package IN_LIST closure OR package MATCHES "^vcpkg-")
            continue()
        endif()
        list(APPEND closure "${package}")
        foreach(dependency IN LISTS depends_${package})
            if(dependency MATCHES "/${TRIPLET}$")
                list(APPEND queue "${dependency}")
            endif()
        endforeach()
    endwhile()
    list(SORT closure)
    set(${out} "${closure}" PARENT_SCOPE)
endfunction()

foreach(variable IN ITEMS INSTALLED_DIR MANIFEST TRIPLET)
    if(NOT ${variable})
        message(FATAL_ERROR "The licence check needs INSTALLED_DIR, MANIFEST and TRIPLET.")
    endif()
endforeach()
if(NOT DEFINED LICENSES)
    set(LICENSES "${CMAKE_CURRENT_LIST_DIR}/licenses.json")
endif()

file(READ "${LICENSES}" licenses)
cmake_path(GET LICENSES FILENAME licenses_name)
irama_json_strings(libirama_allowed "${licenses}" libirama)
irama_json_strings(applications_allowed "${licenses}" applications)
irama_json_strings(font_licenses "${licenses}" fonts licenses)
irama_json_strings(font_ports "${licenses}" fonts ports)

# CMake has no maps, so each override is looked up by its port, in override_license_<port> and
# override_version_<port>.
string(JSON count ERROR_VARIABLE error LENGTH "${licenses}" overrides)
if(error)
    set(count 0)
endif()
set(index 0)
while(index LESS count)
    string(JSON override GET "${licenses}" overrides ${index})
    string(JSON port ERROR_VARIABLE error GET "${override}" port)
    if(error)
        message(FATAL_ERROR "Override ${index} in ${licenses_name} names no port.")
    endif()
    foreach(field IN ITEMS version license reason)
        string(JSON ${field} ERROR_VARIABLE error GET "${override}" ${field})
        if(error OR ${field} STREQUAL "")
            message(FATAL_ERROR "The override for ${port} in ${licenses_name} lacks its ${field}.")
        endif()
    endforeach()
    set(override_version_${port} "${version}")
    set(override_license_${port} "${license}")
    math(EXPR index "${index} + 1")
endwhile()

# Lists the installed ports as <name>/<triplet>, and the ports each depends on as
# depends_<name>/<triplet>, its features' dependencies included. Only the fields read here matter,
# so any `;`, `[` or `]` in the others, which would split a CMake list, is blanked first. vcpkg
# writes no status file until it installs a port.
if(NOT IS_DIRECTORY "${INSTALLED_DIR}/vcpkg")
    message(FATAL_ERROR "No vcpkg installed tree is at ${INSTALLED_DIR}.")
endif()
set(installed "")
if(EXISTS "${INSTALLED_DIR}/vcpkg/status")
    file(READ "${INSTALLED_DIR}/vcpkg/status" status)
    string(REPLACE "\r\n" "\n" status "${status}")
    string(REPLACE ";" " " status "${status}")
    string(REPLACE "[" " " status "${status}")
    string(REPLACE "]" " " status "${status}")
    string(REGEX REPLACE "\n\n+" ";" paragraphs "${status}")
    foreach(paragraph IN LISTS paragraphs)
        set(paragraph "\n${paragraph}\n")
        if(NOT paragraph MATCHES "\nStatus: install ok installed\n"
            OR NOT paragraph MATCHES "\nPackage: ([^\n]+)\n"
        )
            continue()
        endif()
        set(name "${CMAKE_MATCH_1}")
        string(REGEX MATCH "\nArchitecture: ([^\n]+)\n" _ "${paragraph}")
        set(triplet "${CMAKE_MATCH_1}")
        if(NOT paragraph MATCHES "\nFeature: ")
            list(APPEND installed "${name}/${triplet}")
        endif()
        # A dependency for another triplet is named <port>:<triplet>.
        if(paragraph MATCHES "\nDepends: ([^\n]+)\n")
            string(REPLACE ", " ";" dependencies "${CMAKE_MATCH_1}")
            foreach(dependency IN LISTS dependencies)
                if(dependency MATCHES "^([^:]+):(.+)$")
                    list(APPEND depends_${name}/${triplet} "${CMAKE_MATCH_1}/${CMAKE_MATCH_2}")
                else()
                    list(APPEND depends_${name}/${triplet} "${dependency}/${triplet}")
                endif()
            endforeach()
        endif()
    endforeach()
endif()

file(READ "${MANIFEST}" manifest)
set(libirama_roots "")
irama_manifest_names(libirama_roots "${manifest}" dependencies)
irama_manifest_names(libirama_roots "${manifest}" features examples dependencies)
irama_port_closure(libirama_ports ${libirama_roots})

set(problems "")
foreach(package IN LISTS installed)
    string(REGEX REPLACE "/.*" "" name "${package}")
    string(REGEX REPLACE ".*/" "" triplet "${package}")
    file(READ "${INSTALLED_DIR}/${triplet}/share/${name}/vcpkg.spdx.json" spdx)
    # vcpkg lists the port first, before the binary package and the sources.
    string(JSON port GET "${spdx}" packages 0)
    string(JSON version GET "${port}" versionInfo)
    string(JSON license GET "${port}" licenseConcluded)
    set(by_override "")
    if(DEFINED override_license_${name})
        if(NOT version STREQUAL override_version_${name})
            string(CONCAT problem
                "${name} ${version} isn't the version its override was reviewed at, "
                "${override_version_${name}}. Review its licence again, and update the override."
            )
            list(APPEND problems "${problem}")
            continue()
        endif()
        set(license "${override_license_${name}}")
        set(by_override " by its override")
    endif()
    # vcpkg writes NOASSERTION for a port that leaves out its licence, and LicenseRef-vcpkg-null for
    # one that sets it to null.
    if(license MATCHES "^(NOASSERTION|LicenseRef-vcpkg-null)$")
        string(CONCAT problem
            "${name} ${version} declares no licence. "
            "Review it, and add an override for that version."
        )
        list(APPEND problems "${problem}")
        continue()
    endif()
    if(package IN_LIST libirama_ports)
        set(allowed ${libirama_allowed})
        set(list_name "libirama's allowlist")
    else()
        set(allowed ${applications_allowed})
        if(name IN_LIST font_ports)
            list(APPEND allowed ${font_licenses})
        endif()
        set(list_name "the applications' allowlist")
    endif()
    irama_license_allowed(covered "${license}" ${allowed})
    if(NOT covered)
        string(CONCAT problem "${name} ${version} is under ${license}${by_override}, "
            "which ${list_name} doesn't cover."
        )
        list(APPEND problems "${problem}")
    endif()
    set(port_${package} "${port}")
    set(version_${package} "${version}")
    set(license_${package} "${license}")
endforeach()

if(problems)
    irama_fail("The dependencies' licences fail the check:" "" ${problems})
endif()

if(NOT DEFINED CHECK_NOTICES AND NOT DEFINED WRITE_NOTICES)
    return()
endif()
if(NOT APPLICATIONS)
    message(FATAL_ERROR "The notices need APPLICATIONS.")
endif()

# A dependency for other platforms only isn't installed here, but ships there, and the one notices
# file serves every platform. Each port that ships keeps the applications that use it in
# used_by_<name>/<triplet>.
set(shipped "")
set(problems "")
string(REPLACE "," ";" applications "${APPLICATIONS}")
foreach(application IN LISTS applications)
    if(NOT application MATCHES "^([^=]+)=(.+)$")
        message(FATAL_ERROR
            "An application reads \"<feature>=<application>\", not: ${application}"
        )
    endif()
    set(application_name "${CMAKE_MATCH_2}")
    set(roots "")
    irama_manifest_names(roots "${manifest}" features ${CMAKE_MATCH_1} dependencies)
    foreach(root IN LISTS roots)
        if(NOT "${root}/${TRIPLET}" IN_LIST installed)
            list(APPEND problems
                "${root} isn't installed for ${TRIPLET}, so the notices would leave it out."
            )
        endif()
    endforeach()
    irama_port_closure(application_ports ${roots})
    foreach(package IN LISTS application_ports)
        list(APPEND used_by_${package} "${application_name}")
    endforeach()
    list(APPEND shipped ${application_ports})
endforeach()
if(problems)
    irama_fail("The notices can't list every port that ships:" "" ${problems})
endif()
list(REMOVE_DUPLICATES shipped)
list(SORT shipped)

set(notices [[
# Third-party notices

The applications in this repository are built with the third-party libraries listed here. Each is
listed with its version, its licence as an SPDX expression, the applications that use it, and the
notice that its licence asks to be passed on. libirama needs none: each library it uses is under a
licence that asks for no notice.

This file is generated from what the applications' features in `vcpkg.json` depend on, and what
those depend on in turn. Build tools and the tests' dependencies don't ship, so they aren't listed.
To bring it up to date, build the `notices` target rather than editing it.
]])

# Each port's copyright file loses its trailing spaces and the blank lines around it, which a diff
# would flag, and its fence is longer than any run of backticks in it, which would close it early.
# The entry is kept as entry_<name> for the check below.
set(shipped_names "")
foreach(package IN LISTS shipped)
    string(REGEX REPLACE "/.*" "" name "${package}")
    list(APPEND shipped_names "${name}")
    set(entry "\n## ${name}\n\n")
    string(APPEND entry "- Version: ${version_${package}}\n- Licence: ${license_${package}}\n")
    string(JSON homepage ERROR_VARIABLE error GET "${port_${package}}" homepage)
    if(NOT error)
        string(APPEND entry "- Homepage: ${homepage}\n")
    endif()
    list(JOIN used_by_${package} ", " used_by)
    string(APPEND entry "- Used by: ${used_by}\n")

    file(READ "${INSTALLED_DIR}/${TRIPLET}/share/${name}/copyright" text)
    string(REPLACE "\r\n" "\n" text "${text}")
    string(REGEX REPLACE "[ \t]+\n" "\n" text "${text}")
    string(REGEX REPLACE "^\n+" "" text "${text}")
    string(REGEX REPLACE "[ \t\n]+$" "" text "${text}")
    set(fence "```")
    string(REGEX MATCHALL "`+" runs "${text}")
    foreach(run IN LISTS runs)
        string(LENGTH "${run}" length)
        string(LENGTH "${fence}" fence_length)
        if(length GREATER_EQUAL fence_length)
            math(EXPR length "${length} + 1")
            string(REPEAT "`" ${length} fence)
        endif()
    endforeach()
    string(APPEND entry "\n${fence}text\n${text}\n${fence}\n")

    set(entry_${name} "${entry}")
    string(APPEND notices "${entry}")
endforeach()
if(NOT shipped)
    string(APPEND notices "\nThe applications use no third-party libraries.\n")
endif()

if(DEFINED WRITE_NOTICES)
    file(WRITE "${WRITE_NOTICES}" "${notices}")
    return()
endif()

set(existing "")
if(EXISTS "${CHECK_NOTICES}")
    file(READ "${CHECK_NOTICES}" existing)
    string(REPLACE "\r\n" "\n" existing "${existing}")
endif()
if(existing STREQUAL notices)
    return()
endif()
cmake_path(GET CHECK_NOTICES FILENAME notices_name)
set(problems "")
foreach(package IN LISTS shipped)
    string(REGEX REPLACE "/.*" "" name "${package}")
    string(FIND "${existing}" "${entry_${name}}" found)
    if(NOT found EQUAL -1)
        continue()
    endif()
    string(FIND "${existing}" "\n## ${name}\n\n- Version: " found)
    if(found EQUAL -1)
        list(APPEND problems "${notices_name} lacks ${name} ${version_${package}}, which ships.")
    else()
        list(APPEND problems "${notices_name} is out of date for ${name} ${version_${package}}.")
    endif()
endforeach()
string(REGEX MATCHALL "\n## [^\n]+\n\n- Version: [^\n]+" headings "${existing}")
foreach(heading IN LISTS headings)
    string(REGEX MATCH "^\n## ([^\n]+)\n\n- Version: (.+)$" _ "${heading}")
    if(NOT CMAKE_MATCH_1 IN_LIST shipped_names)
        list(APPEND problems
            "${notices_name} lists ${CMAKE_MATCH_1} ${CMAKE_MATCH_2}, which no longer ships."
        )
    endif()
endforeach()
if(NOT problems)
    list(APPEND problems "${notices_name} differs outside the dependencies' entries.")
endif()
irama_fail("${notices_name} is out of date:"
    "Build the notices target to bring it up to date." ${problems}
)
