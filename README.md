<!--
SPDX-FileCopyrightText: 2026 takumu-dev
SPDX-License-Identifier: Zlib
-->

# Irama

A music tracker with built-in synthesisers. The repository holds two parts:

- **libirama**, the library that games and apps embed. Its public interface is the C header
  `irama.h`. It is under the zlib licence, so a game can ship it with no extra legal text.
- **irama-cli**, a command-line tool that drives libirama, under GPL-3.0-or-later.

[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) describes how they fit together.

## Requirements

| Tool | Oldest supported |
|---|---|
| CMake | 4.4 |
| GCC, on Linux | 16 |
| Clang, on Linux | 23 |
| libstdc++, under either Linux compiler | 16 |
| MSVC, on Windows | 14.52 |
| clang-format and clang-tidy, for the checks | 23 |
| Doxygen, for the checks | 1.18 |

Configuring with anything older stops with a message naming the version needed. Both Linux
compilers build against libstdc++. MSVC builds with its own standard library, from a developer
command prompt for MSVC 14.52 or newer, which picks the compiler and the architecture. Ninja and
[vcpkg](https://vcpkg.io) are needed too: every dependency comes from the vcpkg manifest, found
through the `VCPKG_ROOT` environment variable, and the build never downloads anything itself.

## Building

Every supported configuration is a preset in `CMakePresets.json`, and a workflow preset configures,
builds and tests in one step:

```sh
cmake --workflow --preset gcc-debug
```

| Preset | Configuration |
|---|---|
| `gcc-debug`, `gcc-release` | GCC |
| `clang-debug`, `clang-release` | Clang with libstdc++ |
| `msvc-debug`, `msvc-release` | MSVC, on Windows |
| `gcc-libirama` | GCC, libirama alone, built but not tested |
| `lint` | The formatting, with clang-format, and the documentation, with Doxygen |
| `tidy` | Clang, every part built with clang-tidy |

Each preset builds into `build/<preset>/`. Warnings are errors in every preset. Machine-specific
settings, such as `VCPKG_ROOT` or a compiler under another name, belong in a
`CMakeUserPresets.json`, which git ignores.

Each part beside libirama has a switch, on by default, that also asks vcpkg for that part's
dependencies: `IRAMA_BUILD_CLI` and `IRAMA_BUILD_TESTS`. With every switch off, as in
`gcc-libirama`, only libirama and its own dependencies are built. A project that adds Irama as a
subproject gets the same, since the switches are off there by default.

## Tests

The tests run with each workflow preset, or alone with `ctest --preset <preset>`. Every test carries
one CTest label: `abort`, for the checks that end the program, which run through
`cmake/expect_abort.cmake`, or `unit`, for Catch2 test cases and the other checks run through CMake.
Each test's name begins with what it tests: a libirama component such as `base` or `capi`, `cli`
for irama-cli, or `build` for the configuration itself.

## Contributing

[`CODING_STANDARDS.md`](CODING_STANDARDS.md) says how the code is written. Build the `format` target
of any preset to format the sources as the `lint` preset checks them.

Changes reach `develop` and `main` only through pull requests, merged with a merge commit, following
git flow: feature branches into `develop`, and release and hotfix branches into `main` and back into
`develop`.

CircleCI checks every pull request into `develop` or `main`, every merge into them, and `develop`
twice a week:

- Each Linux preset runs as its own job, `cmake --workflow --preset <preset>`, in the image built
  from `ci/linux/`: Docker's official GCC 16 image, with Clang 23 from apt.llvm.org pointed at
  GCC 16's libstdc++, CMake 4.4 and Doxygen 1.18. The image is public, as
  `ghcr.io/takumu-dev/irama-ci`, tagged with the hash that `ci/image-tag.sh` prints, so CI's
  compilers and checks are one `docker pull` away.
- A change under `ci/linux/` makes a new tag. Commit it, then set `image-tag` in
  `.circleci/config.yml` to what `ci/image-tag.sh` prints, or the `image` job fails. That job builds
  and pushes a tag only when it is missing.
- One Windows job, on CircleCI's Windows Server image at a dated tag, runs `msvc-debug`, then
  `msvc-release`, even when the first fails. The scripts in `ci/windows/` install CMake and Ninja,
  check that the image has MSVC 14.52, and enter its developer command prompt. A Windows minute
  costs far more than a Linux one, so the job runs on pull requests and on Monday's schedule, and
  not on merges.
- Test jobs report their JUnit results. A failed job keeps its configure, build, test and vcpkg
  logs, and `build/<preset>/golden-failures/`, as artifacts.
- vcpkg's binaries are cached per image tag, manifest and presets. Linux jobs on `develop` save the
  cache after passing, and pull requests restore it. The Windows job, keyed on the manifest and
  presets alone, saves its cache from any branch once both presets pass.
- The `reuse` job runs `reuse lint` in the FSF's `fsfe/reuse` image, pinned by digest.
- The jobs fan into one check, `ci/circleci: ci`.

Some settings live outside the repository:

- **The CircleCI project** connects through CircleCI's GitHub App. Two triggers follow pull
  requests: one builds each push to a branch with an open pull request, and one builds the branch a
  pull request merges into. Two schedule triggers run the jobs on `develop` every Monday and
  Thursday at 05:00 UTC, and Monday's sets the `windows` pipeline parameter. Nothing else builds,
  and pull requests from forks never do. The `image` job pushes with the token in the organisation's
  `ghcr` context.
- **A GitHub ruleset** on `develop` and `main` requires the `ci/circleci: ci` status from CircleCI's
  GitHub App, so no other app or account can set it. It accepts changes only through pull requests,
  allows only merge commits, and stops either branch being deleted or force-pushed.

## Licences

The repository follows [REUSE](https://reuse.software): every file names its copyright holder and
licence, in its first lines or in `REUSE.toml`, and the licence texts are in `LICENSES/`.
Everything under `cli/` is GPL-3.0-or-later. Everything else, libirama, the tests and the build
files included, is zlib.

Every configuration checks the licences of the dependencies vcpkg installed, against the
allowlists in `cmake/licenses.json`: a strict one for libirama, and a wider one for everything else.
[`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md) holds the notices of the libraries irama-cli
ships. Build the `notices` target of any preset that builds irama-cli to bring it up to date.
