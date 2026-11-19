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
| `capi` | The C API: `irama.h` and its implementation. | nothing |

`irama::engine` links every component but `capi`, which leaves it empty while `capi` is the only
component. Nothing links `capi`: it sits on top of the other components, and its only public header
is `irama.h`, which is C.

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
| `cmake/IramaComponents.cmake` | `irama_add_component()`. |
| `cmake/IramaLint.cmake` | The formatting, clang-tidy and documentation checks. |
| `.clang-format`, `.clang-tidy`, `Doxyfile` | The settings of those checks. |
| `tests/` | The tests, one folder per component under `unit/`, and the CLI and configuration checks. |
