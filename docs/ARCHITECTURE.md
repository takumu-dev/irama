<!--
SPDX-FileCopyrightText: 2026 takumu-dev
SPDX-License-Identifier: Zlib
-->

# Architecture

## Parts

| Part | Folder | What it is | Licence |
|---|---|---|---|
| libirama | `libirama/` | The library that games and apps embed. | zlib |
| irama-cli | `cli/` | A command-line tool built on libirama. | GPL-3.0-or-later |

libirama has two interfaces:

- **The C API**, `libirama/include/irama/irama.h`, is what games and apps use. It is plain C99, so
  a game written in C can include it, and it compiles as C++11 or newer too.
- **The internal C++ API**, the `irama::engine` target, is what irama-cli uses. It promises no
  stability, since irama-cli is only ever built against the libirama beside it.

`cli/` defines executables only, never libraries, so nothing under the GPL can reach libirama.

## Components

libirama is split into components: each folder under `libirama/src/` is its own static library,
`irama::<component>`, declared with `irama_add_component()` from `cmake/IramaComponents.cmake`. A
component's public headers come only with its library, so a component can use another only by
linking it, and the build keeps the layers apart. Every component compiles its own sources without
exceptions or RTTI.

| Component | What it holds | Links |
|---|---|---|
| `base` | Assertions, `irama::narrow`, and the `IRAMA_NONBLOCKING` mark. | nothing |
| `capi` | The C API: `irama.h` and its implementation. | `base` |

`irama::engine` links every component but `capi`. Nothing links `capi`: it sits on top of the other
components, and its only public header is `irama.h`, which is C.

Once the whole project is configured, `irama_check_link_closure()` checks what every target under
`libirama/` links. A libirama target may link other libirama targets, and the imported targets that
`libirama/CMakeLists.txt` lists, which are those of libirama's own dependencies at the top level of
`vcpkg.json`. Any other link stops the configuration, naming it. The list is fixed, rather than
taken from each component's `PACKAGES`, so that no component can admit a package by declaring it.

## Dependencies

Every dependency comes from `vcpkg.json`. libirama's are its top-level dependencies, and each other
part's are a manifest feature. Every configuration checks the licence of each port vcpkg installed
against one of two allowlists in `cmake/licenses.json`:

- **libirama's list** covers libirama's dependencies and every port they depend on. It admits only
  licences that ask a game for no notice, so libirama needs no third-party notices.
- **The applications' list** covers every other port, build tools and the tests' dependencies
  included. Its font licences apply only to the ports it names as fonts.

An override in `cmake/licenses.json` gives a port's licence as a review found it, for the version
reviewed. It can correct what a port declares, but the licence it gives must still be on the port's
list. Configuring fails, naming each port whose list doesn't cover it.

`THIRD_PARTY_NOTICES.md` lists the libraries the applications ship, from their features in
`vcpkg.json` and what those depend on, with the applications that use each. Build the `notices`
target to rewrite it. A test fails when it is out of date.

## Version

libirama and irama-cli share one version, written in `irama.h` as `IRAMA_VERSION_MAJOR`,
`IRAMA_VERSION_MINOR` and `IRAMA_VERSION_PATCH`. The top-level `CMakeLists.txt` reads those macros
to set the project's version, so there is no second copy to keep in step.

- A game compares `irama_version()`, the version of the library it runs with, with
  `IRAMA_VERSION`, the version of the header it compiled against.
- irama-cli doesn't include `irama.h`, which belongs to `capi`. It gets the project's version as
  the compile definition `IRAMA_VERSION_STRING`, and prints it for `irama-cli --version`.

The tests check that all of these agree.

## Build

| Folder or file | What it holds |
|---|---|
| `CMakeLists.txt` | The project, the part switches, and the version read from `irama.h`. |
| `CMakePresets.json` | Every supported configuration, each with a workflow preset. |
| `vcpkg.json` | The dependencies: libirama's at the top level, each other part's as a feature. |
| `cmake/IramaFloors.cmake` | The compiler, libstdc++ and checking tool floor checks. |
| `cmake/IramaConventions.cmake` | The language standard and warning flags of every target. |
| `cmake/IramaComponents.cmake` | `irama_add_component()` and `irama_check_link_closure()`. |
| `cmake/IramaLicenses.cmake` | The licence check at configure, and the `notices` target. |
| `cmake/check_licenses.cmake`, `cmake/licenses.json` | The licence check, and its allowlists. |
| `cmake/IramaLint.cmake` | The formatting, clang-tidy and documentation checks. |
| `.clang-format`, `.clang-tidy`, `Doxyfile` | The settings of those checks. |
| `cmake/expect_abort.cmake` | Runs a program that must abort, and checks its report. |
| `tests/` | The tests, one folder per component under `unit/`, the programs that must abort under `abort/`, and the CLI and configuration checks. |
| `ci/` | The CI's Linux image, under `linux/`, and the Windows job's scripts, under `windows/`. |
| `THIRD_PARTY_NOTICES.md` | The notices of the libraries the applications ship. |
| `REUSE.toml`, `LICENSES/` | The licences of files that can't hold a comment, and their texts. |
