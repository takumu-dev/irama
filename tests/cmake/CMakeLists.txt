# SPDX-FileCopyrightText: 2026 takumu-dev
# SPDX-License-Identifier: Zlib

# The configuration's own behaviour, tested by running its checks as scripts, or by configuring
# small projects that use them.

# Adds a test that runs a floor check through check_floor.cmake with the definitions in ARGN. A
# toolchain below its floor must stop the configuration with a message naming the floor
# (FAILS_WITH); one at its floor must pass silently; one Irama doesn't support must pass with a
# warning (WARNS_WITH).
function(irama_add_floor_test name)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "FAILS_WITH;WARNS_WITH" "")
    set(test_name "build: ${name}")
    set(definitions "")
    foreach(definition IN LISTS arg_UNPARSED_ARGUMENTS)
        list(APPEND definitions "-D${definition}")
    endforeach()
    add_test(NAME "${test_name}"
        COMMAND "${CMAKE_COMMAND}" ${definitions} -P "${CMAKE_CURRENT_SOURCE_DIR}/check_floor.cmake"
    )
    set_tests_properties("${test_name}" PROPERTIES LABELS unit)
    if(DEFINED arg_FAILS_WITH)
        # CTest passes on the message alone, whatever the exit status, and only FATAL_ERROR prints
        # it.
        set_tests_properties("${test_name}" PROPERTIES
            PASS_REGULAR_EXPRESSION "CMake Error.*${arg_FAILS_WITH}"
        )
    elseif(DEFINED arg_WARNS_WITH)
        set_tests_properties("${test_name}" PROPERTIES
            PASS_REGULAR_EXPRESSION "CMake Warning.*${arg_WARNS_WITH}"
            FAIL_REGULAR_EXPRESSION "CMake Error"
        )
    else()
        set_tests_properties("${test_name}" PROPERTIES
            FAIL_REGULAR_EXPRESSION "CMake (Error|Warning)"
        )
    endif()
endfunction()

irama_add_floor_test("configuring with GCC 15 fails naming the floor"
    COMPILER_ID=GNU COMPILER_VERSION=15.2.0
    FAILS_WITH "Irama needs GCC 16 or newer; this is GCC 15\\.2\\.0"
)
irama_add_floor_test("configuring with GCC 16 passes" COMPILER_ID=GNU COMPILER_VERSION=16.0.0)
irama_add_floor_test("configuring with Clang 22 fails naming the floor"
    COMPILER_ID=Clang COMPILER_VERSION=22.1.8
    FAILS_WITH "Irama needs Clang 23 or newer; this is Clang 22\\.1\\.8"
)
irama_add_floor_test("configuring with Clang 23 passes" COMPILER_ID=Clang COMPILER_VERSION=23.0.0)
irama_add_floor_test("configuring with MSVC 14.51 fails naming the floor"
    COMPILER_ID=MSVC COMPILER_VERSION=19.51.36231
    FAILS_WITH "Irama needs MSVC 14\\.52 \\(cl 19\\.52\\) or newer; this is cl 19\\.51\\.36231"
)
irama_add_floor_test("configuring with MSVC 14.52 passes" COMPILER_ID=MSVC COMPILER_VERSION=19.52.0)
irama_add_floor_test("configuring with an unsupported compiler warns, naming the supported ones"
    COMPILER_ID=AppleClang COMPILER_VERSION=21.0.0
    WARNS_WITH "Irama supports GCC, Clang and MSVC; AppleClang 21\\.0\\.0 is untested"
)
irama_add_floor_test("configuring with libstdc++ 15 fails naming the floor"
    LIBSTDCXX_RELEASE=15
    FAILS_WITH "Irama needs libstdc\\+\\+ 16 or newer; this is libstdc\\+\\+ 15"
)
irama_add_floor_test("configuring with libstdc++ 16 passes" LIBSTDCXX_RELEASE=16)
irama_add_floor_test("configuring with another standard library passes" LIBSTDCXX_RELEASE=)
irama_add_floor_test("configuring with clang-format 22 fails naming the floor"
    TOOL=clang-format TOOL_VERSION=22.1.8
    FAILS_WITH "Irama needs clang-format 23 or newer; this is clang-format 22\\.1\\.8"
)
irama_add_floor_test("configuring with clang-format 23 passes"
    TOOL=clang-format TOOL_VERSION=23.0.0
)
irama_add_floor_test("configuring with clang-tidy 22 fails naming the floor"
    TOOL=clang-tidy TOOL_VERSION=22.1.8
    FAILS_WITH "Irama needs clang-tidy 23 or newer; this is clang-tidy 22\\.1\\.8"
)
irama_add_floor_test("configuring with clang-tidy 23 passes" TOOL=clang-tidy TOOL_VERSION=23.0.0)
irama_add_floor_test("configuring with Doxygen 1.17 fails naming the floor"
    TOOL=Doxygen TOOL_VERSION=1.17.0
    FAILS_WITH "Irama needs Doxygen 1\\.18 or newer; this is Doxygen 1\\.17\\.0"
)
irama_add_floor_test("configuring with Doxygen 1.18 passes" TOOL=Doxygen TOOL_VERSION=1.18.0)

# The licence check, run as a script on vcpkg installed trees: some written here into the build
# tree, and one in licenses/notices/ shaped as vcpkg writes it.
set(irama_license_check "${PROJECT_SOURCE_DIR}/cmake/check_licenses.cmake")

# Adds a test that runs the licence check with the options in ARGN, against the licences file
# `licenses`, or Irama's own if that is empty. The check must pass, or, if `fails_with` isn't empty,
# fail with output matching it.
function(irama_add_license_check_test name licenses fails_with)
    set(options ${ARGN})
    if(licenses)
        cmake_path(ABSOLUTE_PATH licenses)
        list(APPEND options "-DLICENSES=${licenses}")
    endif()
    set(test_name "build: ${name}")
    add_test(NAME "${test_name}"
        COMMAND "${CMAKE_COMMAND}" ${options} -P "${irama_license_check}"
    )
    set_tests_properties("${test_name}" PROPERTIES LABELS unit)
    if(fails_with)
        set_tests_properties("${test_name}" PROPERTIES
            PASS_REGULAR_EXPRESSION "CMake Error.*${fails_with}"
        )
    endif()
endfunction()

# Adds a test that runs the licence check on an installed tree holding PORTS, each written
# "<name>:<triplet> <version> <licence expression>" with the version as vcpkg prints it, beside a
# manifest whose top-level dependencies are LIBIRAMA and whose `examples` feature depends on
# EXAMPLES. A dependency written "<name> host" is a build tool. A port that declares no licence has
# the expression NOASSERTION, as vcpkg writes it. The check must pass, or fail with output matching
# FAILS_WITH.
#
#   DEPENDS   what ports depend on, each written "<name>:<triplet> <dependency>, …" as vcpkg's
#             status file lists them, with a dependency for another triplet written
#             <port>:<triplet>
#   LICENSES  the licences file to check against; Irama's own by default
function(irama_add_license_test name)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "LICENSES;FAILS_WITH"
        "PORTS;DEPENDS;LIBIRAMA;EXAMPLES"
    )
    string(MAKE_C_IDENTIFIER "${name}" id)
    set(directory "${CMAKE_CURRENT_BINARY_DIR}/licenses/${id}")
    set(installed "${directory}/installed")
    file(REMOVE_RECURSE "${directory}")
    file(MAKE_DIRECTORY "${installed}/vcpkg")

    foreach(entry IN LISTS arg_DEPENDS)
        if(NOT entry MATCHES "^([^:]+:[^ ]+) (.+)$")
            message(FATAL_ERROR
                "A test dependency reads \"<name>:<triplet> <dependency>, …\", not: ${entry}"
            )
        endif()
        string(REPLACE ":" "/" package "${CMAKE_MATCH_1}")
        set(depends_${package} "${CMAKE_MATCH_2}")
    endforeach()
    set(status "")
    foreach(port IN LISTS arg_PORTS)
        if(NOT port MATCHES "^([^:]+):([^ ]+) ([^ ]+) (.+)$")
            message(FATAL_ERROR
                "A test port reads \"<name>:<triplet> <version> <licence>\", not: ${port}"
            )
        endif()
        set(port_name "${CMAKE_MATCH_1}")
        set(triplet "${CMAKE_MATCH_2}")
        set(version "${CMAKE_MATCH_3}")
        set(license "${CMAKE_MATCH_4}")
        string(APPEND status "Package: ${port_name}\n")
        if(DEFINED depends_${port_name}/${triplet})
            string(APPEND status "Depends: ${depends_${port_name}/${triplet}}\n")
        endif()
        string(APPEND status "Architecture: ${triplet}\nStatus: install ok installed\n\n")
        string(CONFIGURE [[
{
  "packages": [
    {
      "name": "@port_name@",
      "SPDXID": "SPDXRef-port",
      "versionInfo": "@version@",
      "licenseConcluded": "@license@"
    }
  ]
}
]] spdx @ONLY)
        file(WRITE "${installed}/${triplet}/share/${port_name}/vcpkg.spdx.json" "${spdx}")
    endforeach()
    if(status)
        file(WRITE "${installed}/vcpkg/status" "${status}")
    endif()

    foreach(part IN ITEMS LIBIRAMA EXAMPLES)
        set(${part} "")
        foreach(dependency IN LISTS arg_${part})
            if(dependency MATCHES "^(.+) host$")
                list(APPEND ${part} "{ \"name\": \"${CMAKE_MATCH_1}\", \"host\": true }")
            else()
                list(APPEND ${part} "\"${dependency}\"")
            endif()
        endforeach()
        list(JOIN ${part} ", " ${part})
    endforeach()
    string(CONFIGURE [[
{
  "name": "irama",
  "dependencies": [@LIBIRAMA@],
  "features": {
    "examples": { "description": "Examples", "dependencies": [@EXAMPLES@] },
    "cli": { "description": "irama-cli" }
  }
}
]] manifest @ONLY)
    file(WRITE "${directory}/vcpkg.json" "${manifest}")

    irama_add_license_check_test("${name}" "${arg_LICENSES}" "${arg_FAILS_WITH}"
        "-DINSTALLED_DIR=${installed}"
        "-DMANIFEST=${directory}/vcpkg.json"
        -DTRIPLET=x64-linux
    )
endfunction()

irama_add_license_test(
    "a libirama dependency under a licence outside libirama's allowlist fails the check, naming it"
    LIBIRAMA flacish
    PORTS "flacish:x64-linux 1.0 MIT"
    FAILS_WITH "flacish 1\\.0 is under MIT, which libirama's allowlist doesn't cover\\."
)
irama_add_license_test(
    "an application dependency under a licence on the applications' allowlist passes the check"
    PORTS "flacish:x64-linux 1.0 MIT"
)
irama_add_license_test(
    "what a libirama dependency depends on is checked against libirama's allowlist too"
    LIBIRAMA flacish
    PORTS
        "flacish:x64-linux 1.0 Zlib"
        "oggish:x64-linux 1.3.6 BSD-3-Clause"
    DEPENDS "flacish:x64-linux oggish"
    FAILS_WITH "oggish 1\\.3\\.6 is under BSD-3-Clause, which libirama's allowlist doesn't cover\\."
)
irama_add_license_test(
    "an examples dependency under a licence outside libirama's allowlist fails the check, naming it"
    EXAMPLES audioish
    PORTS "audioish:x64-linux 0.11.26 MIT"
    FAILS_WITH "audioish 0\\.11\\.26 is under MIT, which libirama's allowlist doesn't cover\\."
)
irama_add_license_test(
    "libirama's build tools are checked against the applications' allowlist"
    LIBIRAMA flacish "tool host"
    PORTS
        "flacish:x64-linux 1.0 Zlib"
        "tool:x64-linux 2.0 MIT"
        "vcpkg-cmake:x64-linux 2025-08-07 MIT"
        "vcpkg-cmake:x64-linux-release 2025-08-07 MIT"
        "oggish:x64-linux-release 1.3.6 BSD-3-Clause"
    DEPENDS "flacish:x64-linux vcpkg-cmake, oggish:x64-linux-release"
)
irama_add_license_test("a dependency under any licence libirama's allowlist names passes the check"
    LIBIRAMA zlibish mit0ish unlicensed dedicated
    PORTS
        "zlibish:x64-linux 1.3.2#2 Zlib"
        "mit0ish:x64-linux 1.0 MIT-0"
        "unlicensed:x64-linux 1.0 Unlicense"
        "dedicated:x64-linux 1.0 CC0-1.0"
)
irama_add_license_test("a dependency under any licence the applications' allowlist names passes"
    PORTS
        "zlib:x64-linux 1.3.2#2 Zlib"
        "fmt:x64-linux 12.1.0 MIT"
        "mit0ish:x64-linux 1.0 MIT-0"
        "pcg:x64-linux 2022-04-09 BSD-2-Clause"
        "lz4:x64-linux 1.10.0 BSD-3-Clause"
        "catch2:x64-linux 3.16.0 BSL-1.0"
        "earcut-hpp:x64-linux 3.2.4 ISC"
        "spirv-tools:x64-linux 1.4.335.0 Apache-2.0"
        "shader-slang:x64-linux 2026.19 Apache-2.0 WITH LLVM-exception"
        "stb:x64-linux 2024-07-29 CC0-1.0"
        "parallel-hashmap:x64-linux 2.0.0 Unlicense"
)
irama_add_license_test(
    "the check names every dependency under a licence outside its list, not only the first"
    LIBIRAMA flacish
    PORTS
        "readline:x64-linux 8.2 GPL-3.0-or-later"
        "zlib:x64-linux 1.3.2#2 Zlib"
        "libiconv:x64-linux 1.18 LGPL-2.1-or-later"
        "flacish:x64-linux 1.0 BSD-3-Clause"
    FAILS_WITH "\n +- flacish 1\\.0 [^\n]*\n +- libiconv 1\\.18 [^\n]*\n +- readline 8\\.2 "
)
irama_add_license_test(
    "a dependency under a choice of licences passes the check when its list covers one"
    LIBIRAMA drlibs
    PORTS
        "drlibs:x64-linux 2026-03-03 Unlicense OR MIT-0"
        "highway:x64-linux 1.3.0 Apache-2.0 OR BSD-3-Clause"
        "mimalloc:x64-linux 3.1.5 GPL-3.0-only OR MIT"
)
irama_add_license_test(
    "a dependency under several licences at once passes the check when its list covers each"
    PORTS "sdl3:x64-linux 3.4.18 Zlib AND MIT AND Apache-2.0 AND BSD-3-Clause"
)
irama_add_license_test(
    "a dependency under several licences at once fails the check when its list misses one"
    PORTS "qhull:x64-linux 8.0.2 MIT AND GPL-3.0-only"
    FAILS_WITH "qhull 8\\.0\\.2 is under MIT AND GPL-3\\.0-only, which the applications' allowlist"
)
irama_add_license_test(
    "a licence with an exception fails the check unless allowed with that exception"
    PORTS
        "shader-slang:x64-linux 2026.19 Apache-2.0 WITH LLVM-exception"
        "openjdk-headers:x64-linux 21 Apache-2.0 WITH Classpath-exception-2.0"
    FAILS_WITH "openjdk-headers 21 is under Apache-2\\.0 WITH Classpath-exception-2\\.0, which"
)
irama_add_license_test("in a licence expression, AND binds tighter than OR"
    PORTS "libfoo:x64-linux 1.0 Zlib OR MIT AND GPL-3.0-only"
)
irama_add_license_test("in a licence expression, parentheses group licences"
    PORTS "libbar:x64-linux 1.0 (Zlib OR MIT) AND GPL-3.0-only"
    FAILS_WITH "libbar 1\\.0 is under \\(Zlib OR MIT\\) AND GPL-3\\.0-only, which"
)
irama_add_license_test("a licence expression that can't be read fails the check"
    PORTS
        "unclosed:x64-linux 1.0 (MIT OR Zlib"
        "dangling:x64-linux 1.0 MIT AND"
        "bare-exception:x64-linux 1.0 WITH LLVM-exception"
    FAILS_WITH "\n +- bare-exception 1\\.0 [^\n]*\n +- dangling 1\\.0 [^\n]*\n +- unclosed 1\\.0 "
)
irama_add_license_test("a dependency that declares no licence fails the check, naming it"
    PORTS "lodepng:x64-linux 2021-12-04#1 NOASSERTION"
    FAILS_WITH "lodepng 2021-12-04#1 declares no licence\\. Review it, and add an override for"
)
irama_add_license_test(
    "a dependency whose port sets its licence to null fails the check, naming it"
    PORTS "shader-tool:x64-linux 2026.19 LicenseRef-vcpkg-null"
    FAILS_WITH "shader-tool 2026\\.19 declares no licence\\. Review it, and add an override for"
)

# Overrides, from licenses/overrides.json.
irama_add_license_test("an override gives the licence of a libirama dependency that declares none"
    LICENSES licenses/overrides.json
    LIBIRAMA lodepng
    PORTS "lodepng:x64-linux 2021-12-04#1 NOASSERTION"
)
irama_add_license_test(
    "an override corrects the licence a libirama dependency declares"
    LICENSES licenses/overrides.json
    LIBIRAMA misdeclared
    PORTS "misdeclared:x64-linux 2.0 MIT"
)
irama_add_license_test(
    "an override never admits a licence outside libirama's allowlist, naming its dependency"
    LICENSES licenses/overrides.json
    LIBIRAMA relicensed
    PORTS "relicensed:x64-linux 1.0 Zlib"
    FAILS_WITH "relicensed 1\\.0 is under MIT by its override, which libirama's allowlist doesn't"
)
irama_add_license_test(
    "the same override gives the licence of an application dependency"
    LICENSES licenses/overrides.json
    PORTS "relicensed:x64-linux 1.0 Zlib"
)
irama_add_license_test(
    "an override reviewed at another version of its dependency fails the check, naming it"
    LICENSES licenses/overrides.json
    PORTS "lodepng:x64-linux 2025-06-01 Zlib"
    FAILS_WITH
        "lodepng 2025-06-01 isn't the version its override was reviewed at, 2021-12-04#1\\."
)
irama_add_license_test("an override for a dependency that isn't installed is left alone"
    LICENSES licenses/overrides.json
    PORTS "zlib:x64-linux 1.3.2#2 Zlib"
)
irama_add_license_test("an override without the version it was reviewed at fails the check"
    LICENSES licenses/unpinned-override.json
    FAILS_WITH "The override for lodepng in unpinned-override\\.json lacks its version\\."
)

# Font licences, from licenses/fonts.json, which names monoish as a font.
irama_add_license_test("a font under a font licence passes the check"
    LICENSES licenses/fonts.json
    PORTS "monoish:x64-linux 2.304 OFL-1.1"
)
irama_add_license_test("a port that isn't a font fails the check under a font licence, naming it"
    LICENSES licenses/fonts.json
    PORTS "widgetish:x64-linux 1.0 OFL-1.1"
    FAILS_WITH "widgetish 1\\.0 is under OFL-1\\.1, which the applications' allowlist doesn't"
)
irama_add_license_test("a font that libirama depends on fails the check under a font licence"
    LICENSES licenses/fonts.json
    LIBIRAMA monoish
    PORTS "monoish:x64-linux 2.304 OFL-1.1"
    FAILS_WITH "monoish 2\\.304 is under OFL-1\\.1, which libirama's allowlist doesn't cover\\."
)

irama_add_license_test("a tree with nothing installed passes the check")
irama_add_license_check_test(
    "the check fails on a directory that holds no vcpkg installed tree"
    ""
    "No vcpkg installed tree is at[ \n]+.*/licenses/nothing\\."
    "-DINSTALLED_DIR=${CMAKE_CURRENT_BINARY_DIR}/licenses/nothing"
    "-DMANIFEST=${PROJECT_SOURCE_DIR}/vcpkg.json"
    -DTRIPLET=x64-linux
)

# The notices, tested on the fixture in licenses/notices/: ports for x64-linux, among them vcpkg's
# helpers, as when the build tools share the triplet of what ships. Its THIRD_PARTY_NOTICES.md is
# written by hand, for applications built from the `cli` and `gui` features.
set(irama_notices_fixture "${CMAKE_CURRENT_SOURCE_DIR}/licenses/notices")
set(irama_notices_applications "-DAPPLICATIONS=cli=irama-cli,gui=irama-gui")

# Adds a test that runs the licence check on the fixture, and checks that the notices file NOTICES
# lists what the applications in MANIFEST ship, each the fixture's own by default. The check must
# pass, or fail with output matching FAILS_WITH.
#
#   LICENSES  the licences file; Irama's own by default
function(irama_add_notices_test name)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "MANIFEST;NOTICES;LICENSES;FAILS_WITH" "")
    if(NOT DEFINED arg_MANIFEST)
        set(arg_MANIFEST "${irama_notices_fixture}/vcpkg.json")
    endif()
    if(NOT DEFINED arg_NOTICES)
        set(arg_NOTICES "${irama_notices_fixture}/THIRD_PARTY_NOTICES.md")
    endif()
    cmake_path(ABSOLUTE_PATH arg_MANIFEST)
    cmake_path(ABSOLUTE_PATH arg_NOTICES)
    irama_add_license_check_test("${name}" "${arg_LICENSES}" "${arg_FAILS_WITH}"
        "-DINSTALLED_DIR=${irama_notices_fixture}/installed"
        "-DMANIFEST=${arg_MANIFEST}"
        -DTRIPLET=x64-linux
        "${irama_notices_applications}"
        "-DCHECK_NOTICES=${arg_NOTICES}"
    )
endfunction()

irama_add_notices_test(
    "the notices list what the applications' features ship, naming the applications, and no more"
)
irama_add_notices_test("notices that lack a dependency which ships fail the check, naming it"
    MANIFEST licenses/notices/more-dependencies.json
    FAILS_WITH "THIRD_PARTY_NOTICES\\.md lacks test-framework 3\\.16\\.0, which ships\\."
)
irama_add_notices_test(
    "notices that list a dependency which no longer ships fail the check, naming it"
    MANIFEST licenses/notices/fewer-dependencies.json
    FAILS_WITH "THIRD_PARTY_NOTICES\\.md lists pngish 1\\.6\\.59, which no longer ships\\."
)
irama_add_notices_test(
    "notices that name an application which no longer uses a dependency fail the check"
    MANIFEST licenses/notices/fewer-dependencies.json
    FAILS_WITH "THIRD_PARTY_NOTICES\\.md is out of date for fmtish 12\\.1\\.0#1\\."
)
irama_add_notices_test(
    "the notices fail on an application dependency of another platform, which they would leave out"
    MANIFEST licenses/notices/other-platform.json
    FAILS_WITH "windows-only isn't installed for x64-linux, so the notices would leave it out\\."
)
irama_add_notices_test("notices with an out-of-date entry fail the check, naming its dependency"
    LICENSES licenses/notices/relicensed.json
    FAILS_WITH "THIRD_PARTY_NOTICES\\.md is out of date for pngish 1\\.6\\.59\\."
)

set(irama_edited_notices "${CMAKE_CURRENT_BINARY_DIR}/licenses/edited/THIRD_PARTY_NOTICES.md")
file(READ "${irama_notices_fixture}/THIRD_PARTY_NOTICES.md" irama_notices)
string(REPLACE "rather than editing it." "or edit it by hand."
    irama_notices "${irama_notices}"
)
file(WRITE "${irama_edited_notices}" "${irama_notices}")
irama_add_notices_test("notices edited outside the dependencies' entries fail the check"
    NOTICES "${irama_edited_notices}"
    FAILS_WITH "THIRD_PARTY_NOTICES\\.md differs outside the dependencies' entries\\."
)

set(irama_written_notices "${CMAKE_CURRENT_BINARY_DIR}/licenses/written/THIRD_PARTY_NOTICES.md")
set(irama_writing_notices "the notices are written for what the applications ship")
irama_add_license_check_test("${irama_writing_notices}" "" ""
    "-DINSTALLED_DIR=${irama_notices_fixture}/installed"
    "-DMANIFEST=${irama_notices_fixture}/vcpkg.json"
    -DTRIPLET=x64-linux
    "${irama_notices_applications}"
    "-DWRITE_NOTICES=${irama_written_notices}"
)
set_tests_properties("build: ${irama_writing_notices}"
    PROPERTIES FIXTURES_SETUP irama_notices_writing
)
# file(WRITE) ends lines as the platform does, and git ends them with LF in what it commits.
set(irama_comparing_notices "build: the notices written are those checked")
add_test(NAME "${irama_comparing_notices}"
    COMMAND "${CMAKE_COMMAND}" -E compare_files --ignore-eol
        "${irama_written_notices}" "${irama_notices_fixture}/THIRD_PARTY_NOTICES.md"
)
set_tests_properties("${irama_comparing_notices}"
    PROPERTIES LABELS unit FIXTURES_REQUIRED irama_notices_writing
)

# The link-closure check, run by configuring the project in link_closure/ with its CASE. The
# configuration must pass, or fail with a message matching FAILS_WITH.
function(irama_add_link_closure_test name case)
    cmake_parse_arguments(PARSE_ARGV 2 arg "" "FAILS_WITH" "")
    set(test_name "build: ${name}")
    add_test(NAME "${test_name}"
        COMMAND "${CMAKE_COMMAND}" --fresh
            -S "${CMAKE_CURRENT_SOURCE_DIR}/link_closure"
            -B "${CMAKE_CURRENT_BINARY_DIR}/link_closure/${case}"
            -G "${CMAKE_GENERATOR}"
            "-DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}"
            "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
            "-DCASE=${case}"
    )
    set_tests_properties("${test_name}" PROPERTIES LABELS unit)
    if(DEFINED arg_FAILS_WITH)
        set_tests_properties("${test_name}" PROPERTIES
            PASS_REGULAR_EXPRESSION "CMake Error.*${arg_FAILS_WITH}"
        )
    else()
        set_tests_properties("${test_name}" PROPERTIES FAIL_REGULAR_EXPRESSION "CMake Error")
    endif()
endfunction()

irama_add_link_closure_test(
    "libirama targets that link one another and the imported targets allowed pass the check"
    allowed
)
irama_add_link_closure_test(
    "a libirama target that links an imported target not allowed fails the check, naming the link"
    imported
    FAILS_WITH "irama_wav links Fake::forbidden\\."
)
irama_add_link_closure_test(
    "a libirama target that links a library by name fails the check, naming the link"
    library
    FAILS_WITH "irama_wav links m\\."
)
irama_add_link_closure_test(
    "a libirama target that links a target from outside libirama fails the check, naming the link"
    target
    FAILS_WITH "irama_wav links project_helper\\."
)
irama_add_link_closure_test(
    "a libirama target that links through a generator expression fails the check, naming it"
    expression
    # Test properties take generator expressions, so the pattern steers clear of writing one.
    FAILS_WITH "irama_wav links [^ ]+CONFIG:Debug.:Fake::allowed., a generator expression"
)
irama_add_link_closure_test(
    "a libirama target that passes on a link not allowed fails the check, naming the link"
    interface
    FAILS_WITH "irama_base links Fake::forbidden\\."
)
irama_add_link_closure_test(
    "a link added to libirama after libirama is configured fails the check, naming it"
    late
    FAILS_WITH "irama_engine links Fake::forbidden\\."
)

# Irama's own notices, in a configuration that installs what every application ships.
if(DEFINED IRAMA_NOTICES_OPTIONS)
    irama_add_license_check_test("THIRD_PARTY_NOTICES.md lists what the applications ship" "" ""
        ${IRAMA_NOTICES_OPTIONS}
        "-DCHECK_NOTICES=${PROJECT_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
    )
endif()
