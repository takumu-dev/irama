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
| Irama Studio | `studio/` | The tracker itself, a window built on libirama. | GPL-3.0-or-later |

libirama has two interfaces:

- **The C API**, `libirama/include/irama/irama.h`, is what games and apps use. It is plain C99, so
  a game written in C can include it, and it compiles as C++11 or newer too.
- **The internal C++ API**, the `irama::engine` target, is what irama-cli and Studio use. It
  promises no stability, since they are only ever built against the libirama beside them.

`cli/` defines executables only. `studio/` defines the executable `irama-studio` and the static
library `irama_studio_core`, which holds all of Studio but `main()`, so that Studio's tests can link
it. No libirama target links either: the link-closure check below stops the configuration if one
does, so nothing under the GPL can reach libirama.

## Components

libirama is split into components: each folder under `libirama/src/` is its own static library,
`irama::<component>`, declared with `irama_add_component()` from `cmake/IramaComponents.cmake`. A
component's public headers come only with its library, so a component can use another only by
linking it, and the build keeps the layers apart. Every component compiles its own sources without
exceptions or RTTI.

| Component | What it holds | Links |
|---|---|---|
| `base` | Assertions, `irama::narrow`, the `IRAMA_NONBLOCKING` mark, the allocator with the owning pointer and array, the render scope with the real-time guard and the floating-point environment, and the single-producer, single-consumer queue. | nothing |
| `capi` | The C API: `irama.h` and its implementation. | `base` |
| `wav` | The WAV writer, which irama-cli and the tests share. | `base` |

`irama::engine` links every component but `capi`. Nothing links `capi`: it sits on top of the other
components, and its only public header is `irama.h`, which is C.

Once the whole project is configured, `irama_check_link_closure()` checks what every target under
`libirama/` links. A libirama target may link other libirama targets, and the imported targets that
`libirama/CMakeLists.txt` lists, which are those of libirama's own dependencies at the top level of
`vcpkg.json`. Any other link stops the configuration, naming it. The list is fixed, rather than
taken from each component's `PACKAGES`, so that no component can admit a package by declaring it.

## Memory

libirama gets memory only through its allocation entry point, `irama::allocate()` and
`irama::deallocate()` in `<irama/engine/base/allocator.hpp>`. Each takes an `irama::Allocator`: two
callbacks and a pointer of the host's own, so a host can hand libirama an arena or a pool. `free`
is given back the size and alignment the block was allocated with, so an allocator needs no header
of its own, and there is no reallocation, since libirama never grows what it has allocated. A null
block means the allocator has no memory, which reaches the caller as
`irama::AllocationError::out_of_memory`. A size of zero, or an alignment that isn't a power of two,
is a broken precondition, checked with `IRAMA_ASSERT`.

There is no process-wide allocator. Each object that allocates is given one when it is made, and
keeps it to free what it allocated, so two objects can draw on different allocators.
`irama::default_allocator()` draws on the C runtime: `std::aligned_alloc` and `std::free`, or
`_aligned_malloc` and `_aligned_free` on Windows, rounding each size up to a whole number of
alignments.

Memory is owned by one of two types, which free through the allocator they came from, with the size
and alignment they allocated with:

- `irama::OwnedPtr<T>`, from `irama::make_owned<T>()` in `<irama/engine/base/owned_ptr.hpp>`, is a
  `std::unique_ptr` whose deleter holds the allocator.
- `irama::OwnedArray<T>`, from `irama::OwnedArray<T>::create()` in
  `<irama/engine/base/owned_array.hpp>`, is an array whose length is fixed when it is made. It
  stands in for a standard container, which could grow and would draw on the default allocator.

## Real-time checks

Code that a render call runs must not allocate, lock, wait or do I/O. A render entry point holds an
`irama::RenderScope` for as long as it runs, a count of open scopes kept for each thread, so scopes
nest and only the rendering thread is watched. It also carries `IRAMA_NONBLOCKING`, which is
`[[clang::nonblocking]]` on Clang. Two checks share that boundary:

- **The real-time guard**, compiled in by the CMake option `IRAMA_REALTIME_GUARD`, replaces all 20
  forms of the global `operator new` and `operator delete`, and watches libirama's allocation entry
  point. Inside a render scope any of them writes one line to stderr and aborts; elsewhere the
  operators allocate with `malloc`, or `_aligned_malloc` on Windows for the aligned forms, and the
  entry point calls its allocator. The operators sit in `libirama/src/base/render_scope.cpp`, beside the
  scope's constructor, so any program that opens a scope links them. The option is on in the three
  `*-debug` presets and `tidy`, and off by default, so a game's build keeps its own allocator.
- **RealtimeSanitizer**, in `clang-rtsan`, stops the program at the first allocation, lock, system
  call or other unsafe call inside any `IRAMA_NONBLOCKING` function.

The programs under `tests/abort/` prove both: `new` or `delete` inside a render scope, or allocating
or freeing through the entry point, ends with the guard's report where it is compiled in, with
RealtimeSanitizer's under `clang-rtsan`, and goes unchecked elsewhere, while the same outside a
scope, or `new` on a thread that isn't rendering, is allowed.

A render scope also gives every render call one floating-point environment. On x86-64, in every
build, opening a scope saves the thread's MXCSR and sets flush-to-zero and denormals-are-zero, and
closing it puts the saved value back, so scopes nest and the host gets its own environment back. A
render call therefore computes the same, denormals read and written as zero, whatever the host has
set, in irama-cli, in a host's audio callback and in a component's tests alike. The scope's
constructor and destructor stay out of line in `render_scope.cpp`, so no compiler moves
floating-point work across the writes to MXCSR. On other targets a scope leaves the environment as
it is.

Threads talk to the audio thread only through `irama::SpscQueue`, in
`<irama/engine/base/spsc_queue.hpp>`: a fixed ring of trivially copyable messages from one producer
thread to one consumer thread, either of which may be the audio thread. A push or a pop allocates
nothing and never waits; it reports a full or empty queue at once. Its tests push and pop inside a
render scope, so the checks above watch them, and a stress test passes messages between two threads,
which **ThreadSanitizer**, in `clang-tsan`, watches for races.

## The WAV writer and golden tests

`irama::WavWriter`, in `<irama/engine/wav/wav_writer.hpp>`, writes interleaved 32-bit float samples
to a WAV file, as WAV's IEEE float format with a `fact` chunk. It writes through `<cstdio>` and
allocates nothing itself, though the C library may buffer the file. It writes a header for no
samples when it opens the file, and the real lengths when `finish()` closes it. It is part of the
internal C++ API, not of `irama.h`. Nothing in libirama reads WAV files.

`irama-cli tone <file.wav> [--freq <hertz>] [--seconds <seconds>]` writes a sine wave at half of
full scale, 440 Hz and one second by default, the same on both channels, at 48 kHz. It renders
each block of frames inside a render scope, as a render call would, so the real-time checks watch
it and it renders in a render call's floating-point environment.

A golden test runs irama-cli and compares the file it writes with a golden stored beside the test,
`tests/golden/<case>.wav`. `cmake/golden_test.cmake` runs the case, and `irama_wav_compare`, a test
program that reads both files with dr_wav, compares them. dr_wav was written apart from Irama's
writer, so it checks the writer's headers too. The format, rate, channel count and length must
match exactly, and each sample may differ from its golden by at most 1e-5. That absorbs the few
ulps by which compilers' float results differ, so every test preset runs the goldens: the debug
presets under the real-time guard, and the sanitizer presets under their checks. A golden may be at
most 512 KiB, about 1.3 seconds, unless its case allows more. Only the `gcc-release` build writes
goldens, so that they don't drift between compilers. The checks under `tests/golden/harness/` run
the script and the compare tool on goldens wrong in each way, and show that each one fails.

## Irama Studio

Studio opens one window, `Irama Studio`, 1280×800 logical pixels when it opens and resizable, and
draws [Dear ImGui](https://github.com/ocornut/imgui) in it through SDL3's GPU API, SDL_GPU, which on
Linux runs on Vulkan. So running Studio on Linux needs a Vulkan driver.

- **The loop.** Studio has its own `main()` and a plain loop: handle SDL's events, draw a frame, and
  present it with vsync, so it redraws every vsync while the window can be seen. While the window is
  minimised or hidden, the loop waits for the next event instead.
- **Docking.** Windows dock inside the main window only, in a dock space that fills it between the
  main menu bar and the status bar. ImGui's windows of their own, outside the main window, are off:
  they work only under some of SDL's video drivers, and never on native Wayland.
- **The default layout**: File (Quit), View (one toggle for each window) and Help (About) in the
  main menu bar; the Instrument window docked on the left, at two thirds of the width, and the Audio
  window on the right; and the status bar along the bottom. ImGui writes no `imgui.ini`, so Studio
  starts with this layout each time.
- **About** shows Studio's name, the release version, its licence, the repository, and a line
  naming `THIRD_PARTY_NOTICES.md`.
- **HiDPI.** The window is created with SDL's high pixel density, and its sizes are logical pixels.
  ImGui works in the window's own coordinates, and its SDL backend draws at the window's pixel
  density, so Studio scales the window, the style's sizes and the fonts by the rest of the display
  scale: the content scale the user chose. It scales a fresh style each time the display scale
  changes.
- **The font** is Inter Regular at 15 logical pixels, from the `inter` overlay port. The build writes
  the TTF into a generated source as a byte array, with `cmake/embed_file.cmake`, and ImGui loads it
  from memory, so there is no font file to find beside the executable. The colours are ImGui's own
  dark style.
- **Start-up failure.** If SDL's video, the window or the GPU device fails, Studio shows SDL's
  error in a message box, prints it to stderr, and exits with status 1.

`irama_studio_core` is laid out in folders, which are not targets of their own: `app/` (the loop,
the window, the command line), `ui/` (the theme, the font and helpers) and `views/` (what the
window shows). `irama-studio` is `main.cpp` alone. Studio's tests are one Catch2 program,
`irama_studio_tests` in `tests/studio/`, which tests what needs no window. One of its tests checks
that SDL was built with its `wayland`, `x11` and `offscreen` video drivers, since SDL quietly leaves
out a driver whose development packages are missing.

`irama-studio --exit-after-frames <count>` quits after drawing that many frames. The smoke test,
`cmake/smoke_test.cmake`, runs it for 300 frames under SDL's `offscreen` video driver, with no
display, and passes only if Studio exits with status 0 and writes nothing to stderr. SDL_GPU draws on
the machine's own Vulkan driver, and on lavapipe, Mesa's Vulkan driver on the CPU, in CI. The test
carries the `smoke` label, and runs in every test preset that builds Studio but `clang-tsan`: SDL3
and the Vulkan driver aren't built with ThreadSanitizer, so their threads would give false reports.

## Dependencies

Every dependency comes from `vcpkg.json`. libirama's are its top-level dependencies, and each other
part's are a manifest feature. Studio's are SDL3, with its default features off and `vulkan`, plus
`wayland` and `x11` on Linux; Dear ImGui's docking branch, with its SDL3 and SDL_GPU backends; and
Inter. The overlay ports in `ports/`, which `vcpkg-configuration.json` puts ahead of vcpkg's own,
stand in for ports that vcpkg lacks or that don't yet do what Irama needs:

- `drlibs` pins dr_libs to a newer upstream commit than vcpkg's port has, for the bounds fixes in
  dr_flac 0.13.4 and after.
- `imgui` is vcpkg's own port with one change: its SDL3 backends ask for SDL3 without its default
  features. vcpkg's port asks for them, and so would bring in D-Bus, ibus and their own dependencies,
  whatever `vcpkg.json` says. Its files keep vcpkg's MIT licence.
- `inter` installs Inter Regular, the static TTF from the release, with its licence, and a CMake
  package that gives the TTF's path. vcpkg has no port for it.

Every configuration checks the licence of each port vcpkg installed against one of two allowlists
in `cmake/licenses.json`:

- **libirama's list** covers libirama's dependencies and every port they depend on. It admits only
  licences that ask a game for no notice, so libirama needs no third-party notices.
- **The applications' list** covers every other port, build tools and the tests' dependencies
  included. Its font licences apply only to the ports it names as fonts.

An override in `cmake/licenses.json` gives a port's licence as a review found it, for the version
reviewed. It can correct what a port declares, but the licence it gives must still be on the port's
list. Configuring fails, naming each port whose list doesn't cover it.

`THIRD_PARTY_NOTICES.md` lists the libraries the applications ship, from their features in
`vcpkg.json` and what those depend on, with the applications that use each. Build the `notices`
target to rewrite it, in a configuration that builds every application. A test there fails when it
is out of date.

## Version

libirama, irama-cli and Studio share one version, written in `irama.h` as `IRAMA_VERSION_MAJOR`,
`IRAMA_VERSION_MINOR` and `IRAMA_VERSION_PATCH`. The top-level `CMakeLists.txt` reads those macros
to set the project's version, so there is no second copy to keep in step.

- A game compares `irama_version()`, the version of the library it runs with, with
  `IRAMA_VERSION`, the version of the header it compiled against.
- irama-cli doesn't include `irama.h`, which belongs to `capi`. It gets the project's version as
  the compile definition `IRAMA_VERSION_STRING`, and prints it for `irama-cli --version`. Studio
  gets it the same way, and shows it in its About window.

The tests check that all of these agree.

## Build

| Folder or file | What it holds |
|---|---|
| `CMakeLists.txt` | The project, the part switches (`IRAMA_BUILD_CLI`, `IRAMA_BUILD_STUDIO` and `IRAMA_BUILD_TESTS`), the real-time guard's switch, and the version read from `irama.h`. |
| `CMakePresets.json` | Every supported configuration, each with a workflow preset. |
| `vcpkg.json` | The dependencies: libirama's at the top level, each other part's as a feature. |
| `vcpkg-configuration.json`, `ports/` | The overlay ports, which stand in for vcpkg's own. |
| `cmake/IramaFloors.cmake` | The compiler, libstdc++ and checking tool floor checks. |
| `cmake/IramaConventions.cmake` | The language standard and warning flags of every target. |
| `cmake/IramaComponents.cmake` | `irama_add_component()` and `irama_check_link_closure()`. |
| `cmake/IramaLicenses.cmake` | The licence check at configure, and the `notices` target. |
| `cmake/check_licenses.cmake`, `cmake/licenses.json` | The licence check, and its allowlists. |
| `cmake/IramaLint.cmake` | The formatting, clang-tidy and documentation checks. |
| `.clang-format`, `.clang-tidy`, `Doxyfile` | The settings of those checks. |
| `cmake/expect_abort.cmake` | Runs a program that must abort, and checks its report. |
| `cmake/golden_test.cmake` | Runs a golden case, or writes its golden. |
| `cmake/smoke_test.cmake` | Starts Studio headless and checks that it quits cleanly. |
| `cmake/embed_file.cmake` | Writes a file's bytes into a C++ source, for Studio's font. |
| `studio/` | Irama Studio: `irama_studio_core`, `irama-studio`, and the `.clang-tidy` that lets ImGui's printf-style functions through. |
| `tests/` | The tests, one folder per component under `unit/`, the programs that must abort under `abort/`, the golden cases and the compare tool under `golden/`, Studio's tests under `studio/`, and the CLI and configuration checks. |
| `ci/` | The CI's Linux image, under `linux/`, the Windows job's scripts, under `windows/`, and the scripts the Linux jobs share. |
| `THIRD_PARTY_NOTICES.md` | The notices of the libraries the applications ship. |
| `REUSE.toml`, `LICENSES/` | The licences of files that can't hold a comment, and their texts. |
| `.gitattributes` | Marks WAV files binary. |
