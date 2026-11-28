<!--
SPDX-FileCopyrightText: 2026 takumu-dev
SPDX-License-Identifier: Zlib
-->

# Irama

A music tracker with built-in synthesisers. The repository holds three parts:

- **libirama**, the library that games and apps embed. Its public interface is the C header
  `irama.h`. It is under the zlib licence, so a game can ship it with no extra legal text.
- **irama-cli**, a command-line tool that drives libirama, under GPL-3.0-or-later.
- **Irama Studio**, the tracker itself, a window drawn with SDL3 and Dear ImGui, under
  GPL-3.0-or-later. It is built and checked on Linux.

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

vcpkg builds SDL3 for Studio from source, against the system's Wayland and X11 development files:
on Debian or Ubuntu, `libegl-dev`, `libwayland-dev`, `libx11-dev`, `libxext-dev` and
`libxkbcommon-dev`. Without them SDL leaves out that video driver, and a test fails. Studio draws
through Vulkan, so running it, or its smoke test, needs a Vulkan driver.

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
| `clang-asan` | Clang, debug, with AddressSanitizer and UndefinedBehaviorSanitizer |
| `clang-tsan` | Clang, debug, with ThreadSanitizer |
| `clang-rtsan` | Clang, debug, with RealtimeSanitizer |
| `gcc-libirama` | GCC, libirama alone, built but not tested |
| `lint` | The formatting, with clang-format, and the documentation, with Doxygen |
| `tidy` | Clang, every part built with clang-tidy |

Each preset builds into `build/<preset>/`. Warnings are errors in every preset. Machine-specific
settings, such as `VCPKG_ROOT` or a compiler under another name, belong in a
`CMakeUserPresets.json`, which git ignores.

Each part beside libirama has a switch, on by default, that also asks vcpkg for that part's
dependencies: `IRAMA_BUILD_CLI`, `IRAMA_BUILD_STUDIO` and `IRAMA_BUILD_TESTS`. With every switch
off, as in `gcc-libirama`, only libirama and its own dependencies are built. A project that adds
Irama as a subproject gets the same, since the switches are off there by default. The `msvc-*`
presets never look for clang-format or Doxygen, which they don't run, so an older one on the `PATH`,
such as Visual Studio's own clang-format, can't stop them configuring.

A build with Studio writes it to `build/<preset>/studio/irama-studio`. It starts with the
Instrument and Audio windows docked side by side, and remembers its window and layout in
`$XDG_CONFIG_HOME/irama/`, or `~/.config/irama/`, on Linux, and in `%APPDATA%\irama\` on Windows.
It draws through Vulkan on Linux and Direct3D 12 on Windows. It takes two options: `--config-dir <directory>`
keeps the settings in another directory, for a clean start, and `--exit-after-frames <count>`, for
tests, quits after drawing that many frames.

On Linux Studio plays through PulseAudio, which PipeWire serves too, or through ALSA where there is
no PulseAudio server, and on Windows through WASAPI, in shared mode. It runs silent where none of
them works. The Audio window picks the device, the
system default unless another is chosen, and the buffer size, 256 frames unless another is chosen.
A device that stops playing, such as a USB device unplugged, gives way to the system default, and
where that fails too, the Audio window's Reopen button tries again. The computer
keyboard is a piano of two rows: `Z` to `/` from C of the keyboard octave, and `Q` to `P` from the
octave above, by the keys' positions on a US keyboard, so every layout plays the same. Numpad `/`
and `*`, the Keyboard menu, or the buttons in the status bar, lower and raise the octave, from 0 to 8, so that at the
default of 4, `Z` plays C-4.

`IRAMA_REALTIME_GUARD` compiles in the real-time guard, which stops the program when code inside a
render call allocates or frees memory. It is off by default, and on in the three `*-debug` presets
and `tidy`.

## Tests

The tests run with each workflow preset, or alone with `ctest --preset <preset>`. Every test carries
one CTest label: `abort`, for the checks that end the program, which run through
`cmake/expect_abort.cmake`; `golden`, for the golden cases, which compare what irama-cli writes with
a stored WAV file; `smoke`, for the test that starts Studio, with no display under SDL's `offscreen`
video driver on Linux and in a window of its own on Windows, and checks that it quits cleanly after
300 frames; or `unit`, for Catch2 test cases and the other checks run through CMake. Each test's
name begins with what it tests: a libirama component such as `base` or `capi`, `cli` for irama-cli,
`studio` for Studio, `golden` for a golden case, `golden harness` for the checks of the golden tests
themselves, or `build` for the configuration itself. The `clang-tsan` test preset leaves out the
`smoke` test, since SDL3 and the Vulkan driver aren't built with ThreadSanitizer. Every test preset
stops a test that runs for more than 5 minutes, and runs CTest without interactive debugging, so on
Windows no error dialog waits for a click. For the same reason, each program's `main` sends the MSVC
debug runtime's assertion and error reports to stderr.

`IRAMA_UPDATE_GOLDENS=1 ctest --preset gcc-release -L golden` writes the goldens. Listen to each
new or changed golden before committing it. [`CODING_STANDARDS.md`](CODING_STANDARDS.md) has the
whole procedure.

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
  GCC 16's libstdc++, CMake 4.4, Doxygen 1.18, the packages SDL3 needs for its Wayland and X11
  video drivers, and lavapipe, Mesa's Vulkan driver on the CPU, for Studio's smoke test. The image is public, as
  `ghcr.io/takumu-dev/irama-ci`, tagged with the hash that `ci/image-tag.sh` prints, so CI's
  compilers and checks are one `docker pull` away.
- `clang-tsan` runs the same image on a CircleCI machine rather than in its Docker executor. The
  job first lowers the host's ASLR entropy to 28 bits, since ThreadSanitizer supports no more than
  30 on x86_64, and Docker's default seccomp profile forbids its fallback of running with ASLR off.
  It restores the other Linux jobs' vcpkg cache but never saves it.
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
Everything under `cli/` and `studio/` is GPL-3.0-or-later. The overlay of vcpkg's `imgui` port,
in `ports/imgui/`, keeps vcpkg's MIT licence. Everything else, libirama, the tests and the build
files included, is zlib.

Every configuration checks the licences of the dependencies vcpkg installed, against the
allowlists in `cmake/licenses.json`: a strict one for libirama, and a wider one for everything else.
[`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md) holds the notices of the libraries that irama-cli
and Studio ship, and ships beside them. Build the `notices` target of any preset that builds both to
bring it up to date.
