<!--
SPDX-FileCopyrightText: 2026 takumu-dev
SPDX-License-Identifier: Zlib
-->

# Coding standards

How code in Irama is written. Where a tool can hold a rule, it does, and these files are the record
of those rules: `.clang-format`, `.clang-tidy` and the one beside `irama.h`, `.editorconfig`, the
warning flags in `cmake/IramaConventions.cmake`, and the `Doxyfile`. This document covers the rest,
and says why. Anything it leaves open follows the
[C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines); where it
departs from them, it says so.

## Language and build

- libirama, irama-cli and the tests are C++23, as ordinary headers rather than C++ modules, with no
  compiler extensions. The C API, `irama.h`, is C99, and compiles as C++11 or newer too.
- The supported compilers are GCC and Clang on Linux, both with libstdc++, and MSVC on Windows,
  with its own standard library. Their floors are GCC 16, Clang 23, libstdc++ 16 and MSVC 14.52,
  and CMake's is 4.4. Floors never fall, and configuring with anything older stops with a message
  naming the floor, rather than failing somewhere deep in a standard header.
- MSVC builds in its conformance mode, `/permissive-`, with `/Zc:__cplusplus`, so code that only one
  compiler accepts fails on the others.
- Every supported configuration is a preset in `CMakePresets.json`, with a workflow preset, so that
  a developer and CI build alike.
- The build finds the libraries it depends on only with `find_package`, and never downloads
  anything. Irama's
  own builds take them from the vcpkg manifest, where each part beside libirama asks for its own as
  a feature, so a game building libirama alone gets only libirama's.

## Components

- libirama is split into components. Each folder under `libirama/src/` is a static library,
  `irama::<component>`, declared with `irama_add_component()`. Its public headers sit under
  `include/irama/engine/<component>/` in its folder, and its private headers beside its sources.
- A component includes only the headers of the components it links. Its public include tree comes
  only with its library, so the build keeps the layers apart rather than review.
- `capi` is the C API. It sits on top of the other components rather than among them, and nothing
  links it. `irama::engine` links every other component, and is the internal C++ API that
  irama-cli uses.
- `cli/` defines executables only, never libraries, so nothing under the GPL can reach libirama.
- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) lists the components and what each one links.

## Naming and spelling

- `PascalCase` for types, concepts, type aliases and template parameters.
- `snake_case` for functions, variables, parameters, namespaces, enumerators and `constexpr`
  constants, with no `k` prefix. This matches the standard library, which doesn't single constants
  out by case.
- Private and protected data members end in an underscore (`frames_`). A plain struct's fields are
  bare.
- Enumerations are `enum class`.
- Macros only where nothing else will do, named `IRAMA_UPPER_CASE`.
- libirama's C++ code is in the namespace `irama`, and its internals in `irama::detail`. There is no
  namespace per component, since targets and include paths already keep components apart. irama-cli
  is a client of libirama, not part of it, so it has a namespace of its own, `irama_cli`.
- The C API has rules of its own, under [C API](#c-api).
- Prose is British: comments, documentation, the README and commit messages. Every identifier is
  American, so a comment that "normalises the colour" sits above `normalize_color`. Identifiers sit
  beside the standard library's, which spell the American way, and one rule for every identifier
  means no word needs a judgement call.
- Names from Irama's domain use its terms, and never the words they replace:

  | Say | Not |
  |---|---|
  | line, one time step of a pattern | row, step |
  | sequence slot, one place in the song's order | order position |
  | real-time guard | allocation guard, audio-thread check |
  | release version, the one version every part shares | library version, app version |
  | format version, the version in an `.irama` file | file version |

## Files and formatting

- Files are `snake_case.hpp` and `snake_case.cpp`, with one main type per header, named after it.
  C++ headers use `#pragma once`. `irama.h` uses an include guard, since it is C.
- Public headers are included as `<irama/...>`, and a target's private headers as `"..."`.
- `clang-format` decides the layout of C and C++ alike, and groups includes as: the file's own
  header, Irama's headers, third-party headers, then the standard library. Build the `format`
  target to apply it, and the `lint` preset to check it. Where a local clang-format formats
  differently from CI's, CI's image decides.
- `.editorconfig` gives every other file its indentation and line endings.

## Errors and exceptions

- A failure that correct code can meet is returned as a value: the function returns
  `[[nodiscard]] std::expected<T, E>`, and its caller checks it before reading it. This departs from
  the Core Guidelines' E.2. Constructors never fail: anything that can fail to come into being is
  made by a factory function.
- No `throw`, `try` or `catch` in libirama or its tests. libirama's components build their own
  sources without exceptions or RTTI in every build, so a stray `throw` or `dynamic_cast` fails
  everywhere, not only in one configuration.
- irama-cli keeps exceptions on, and lets a standard-library exception end the program. It catches
  nothing without a comment saying why.
- `noexcept` goes on move operations and `swap`, where clang-tidy's
  `performance-noexcept-move-constructor` and `performance-noexcept-swap` ask for it, and nowhere
  else. With exceptions off, it would change nothing elsewhere but type traits, and would be noise
  on every declaration.

## Ownership and types

- Values first, and `std::unique_ptr` for a single owner. No `std::shared_ptr` without a comment
  saying why, and no bare `new` or `delete`.
- `T*` and `T&` never own.
- Counts and container indices are `std::size_t`, departing from the Core Guidelines' ES.107.
  Signed types are for arithmetic that can go negative.
- A plain struct gives each member an initialiser, so that a designated initialiser may leave any
  of them out.

## C API

- `irama.h` compiles as C99 and as C++11 or newer, so a game in C or in any C++ can include it. The
  `capi` tests compile it as strict C99, or as C11 on MSVC, which has no C99 mode.
- Its functions and types are `irama_*`, and its enumerators and macros `IRAMA_*`. The
  `.clang-tidy` beside it holds it to these, and turns off the checks that would have it use what
  only C++ has. clang-tidy reads that file only for the file it is run on, so the build checks
  `irama.h` on its own, and the C++ sources that include it skip it.
- Every declaration in `irama.h` has a `///` comment, and the file a `\file` comment: functions,
  types, struct fields, enumerators and macros, the version macros included. It is the reference a
  game developer reads in their editor.
- Each function's comment states:
  - which thread may call it, the audio thread or the control thread;
  - whether it allocates;
  - who owns each pointer argument, and for how long;
  - what it does with `NULL`;
  - every error it can return, each as `\retval`;
  - each parameter, as `\param[in]` or `\param[out]`.
- The comments describe behaviour as a game sees it, and never name libirama's internals.

## Warnings and clang-tidy

- The compiler warnings in `cmake/IramaConventions.cmake` are errors in every preset, through
  `CMAKE_COMPILE_WARNING_AS_ERROR`. Give a C++ target of irama-cli or the tests its settings with
  `irama_target_conventions()`; a component gets them from `irama_add_component()`. The settings are
  private to each target, so none of them reaches a game's own targets. Third-party headers come in
  as system includes, so the warnings apply to Irama's own code alone.
- clang-tidy runs the groups in `.clang-tidy` on every C++ source, with every finding an error,
  in the `tidy` preset. The C sources aren't checked: there are too few of them to be worth a
  configuration of their own.
- A finding that has to stand is suppressed with `NOLINT(<check>): <reason>` or
  `NOLINTNEXTLINE(<check>): <reason>`, never a bare `NOLINT`. Keep the reason on that line:
  `NOLINTNEXTLINE` covers only the line after its own, and any longer explanation goes in a comment
  above it.
- A `.clang-tidy` below the root comes with a comment saying why.

## Documentation

- Every declaration in a component's public headers has a `///` comment, and each such header
  begins with a `\file` comment. `irama.h` follows the [C API](#c-api)'s contract.
- Doxygen reads `irama.h` and every component's public headers, and fails on any warning, an
  undocumented declaration among them; the `lint` preset runs it. Doxygen skips the members of a
  file that has no `\file` comment, which is why each header needs one. In the Clang presets,
  `-Wdocumentation` checks each comment against what it documents, such as a `\param` naming no
  parameter.
- A comment says why, not what.

## Tests

- Tests come first: a test that fails, then the code that passes it. A new test that passes at once
  is checked by breaking the code it covers, since a test that can't fail proves nothing.
- A test exercises a public interface only, and survives any refactoring that keeps the behaviour.
- A test case's name is a sentence saying what holds, such as "irama_version reports the version
  irama.h declares".
- A component's test cases are added with `irama_add_tests()`, as a program that links only that
  component. Each case carries one Catch2 tag naming its component (`[capi]`), and its CTest name
  begins with the same component, which `irama_add_tests()` adds. Checks of the configuration begin
  with `build`, and of irama-cli with `cli`.
- A test file is `<thing>_tests.cpp`, in its component's folder under `tests/unit/`. The check that
  `irama.h` compiles as C99 is the one C source among them, `irama_h_c99_tests.c`, a program of its
  own registered with CTest under the `capi` name. MSVC has no C99 mode, so it builds that file as
  C11.
- Every Catch2 program runs from `tests/catch_main.cpp`, not Catch2's own main. It turns off MSVC's
  abort dialog, as irama-cli's main does, so a test that aborts fails at once rather than wait for a
  click.

## Commits and branches

- Commit subjects follow [Conventional Commits](https://www.conventionalcommits.org),
  `type(scope)!: description`, with the type one of `build`, `chore`, `ci`, `docs`, `feat`, `fix`,
  `perf`, `refactor`, `revert`, `style` or `test`. The scope and the `!` of a breaking change are
  optional. A message describes the change and why.
- Branches follow git flow. `main` holds released versions only, and each merge into it is tagged
  with its version. `develop` is where work comes together. A change is made on `feature/<slug>`
  from `develop`, its slug naming the change; a release on `release/<version>`; and a fix to a
  release on `hotfix/<version>` from `main`. Every merge is `--no-ff`.
- Every branch reaches `develop` or `main` through a pull request, which must pass the
  `ci/circleci: ci` check.
