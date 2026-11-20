<!--
SPDX-FileCopyrightText: 2026 takumu-dev
SPDX-License-Identifier: Zlib
-->

# Changelog

Every notable change to Irama is written here. The format follows
[Keep a Changelog 1.1.0](https://keepachangelog.com/en/1.1.0/), with each release's entries grouped
by part, and the version follows [Semantic Versioning 2.0.0](https://semver.org/spec/v2.0.0.html).
Only libirama's public surface, its C API in `irama.h` and the files it reads, decides the
version. Changes to the internal C++ API and to irama-cli's command line are listed too, but never
force a major version.

## [0.1.0] - 2026-11-20

The first release: libirama's version API and base utilities, the WAV writer, and irama-cli's
`--version` and `tone` commands.

### libirama

#### Added

- `irama.h`, the C API, which compiles as C99 and as C++11 or newer. It holds the version macros
  `IRAMA_VERSION_MAJOR`, `IRAMA_VERSION_MINOR` and `IRAMA_VERSION_PATCH`, `IRAMA_MAKE_VERSION()`,
  `IRAMA_VERSION`, and `irama_version()`, which returns the version libirama was built with.
- `IRAMA_ASSERT`, which checks a precondition in every build, and `IRAMA_DEBUG_ASSERT`, which
  checks only where `NDEBUG` isn't defined. A failed check writes its file, line, condition and
  message to stderr and aborts.
- `irama::narrow`, which converts one integer type to another and asserts that the value fits.
- `irama::RenderScope`, which marks a thread as rendering. With the `IRAMA_REALTIME_GUARD` option
  on, any allocation or free through the global `operator new` or `operator delete` inside a scope
  writes a line to stderr and aborts.
- `IRAMA_NONBLOCKING`, which marks a function that must allocate nothing, lock nothing and never
  wait. On Clang it is `[[clang::nonblocking]]`, which RealtimeSanitizer checks.
- `irama::SpscQueue`, a fixed-capacity queue from one producer thread to one consumer thread whose
  push and pop allocate nothing, take no lock and never wait.
- `irama::WavWriter`, which writes interleaved 32-bit float samples to a WAV file and returns its
  failures as values.
- The CMake build, with a workflow preset for each supported configuration: GCC 16 or newer and
  Clang 23 or newer on Linux, MSVC 14.52 or newer on Windows, AddressSanitizer with
  UndefinedBehaviorSanitizer, ThreadSanitizer and RealtimeSanitizer, libirama on its own, and the
  formatting, clang-tidy and documentation checks. Dependencies come from the vcpkg manifest, and configuring checks their licences.

### irama-cli

#### Added

- `irama-cli --version`, which prints `irama-cli <version>`.
- `irama-cli tone <file.wav> [--freq <hertz>] [--seconds <seconds>]`, which writes a sine wave at
  half of full scale as a 48 kHz stereo float32 WAV file, 440 Hz and one second long by default.

[0.1.0]: https://github.com/takumu-dev/irama/tree/0.1.0
