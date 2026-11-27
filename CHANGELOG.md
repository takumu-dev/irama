<!--
SPDX-FileCopyrightText: 2026 takumu-dev
SPDX-License-Identifier: Zlib
-->

# Changelog

Every notable change to Irama is written here. The format follows
[Keep a Changelog 1.1.0](https://keepachangelog.com/en/1.1.0/), with each release's entries grouped
by part, and the version follows [Semantic Versioning 2.0.0](https://semver.org/spec/v2.0.0.html).
Only libirama's public surface, its C API in `irama.h` and the files it reads, decides the
version. Changes to the internal C++ API, to irama-cli's command line and to Irama Studio are
listed too, but never force a major version.

## [0.2.0] - 2026-11-28

The first sound: libirama's player and basic synth, with band-limited oscillators, a state
variable filter, smoothed parameters and voice stealing; irama-cli's `synth` command; and the
first Irama Studio, which plays the synth from the computer keyboard.

### libirama

#### Added

- `irama::Allocator`, two callbacks and a pointer of the host's own, through which libirama gets
  all of its memory with `irama::allocate()` and `irama::deallocate()`. `default_allocator()` uses
  the C runtime's aligned allocation. `irama::OwnedPtr`, made by `irama::make_owned()`, and
  `irama::OwnedArray` own memory from an allocator and give it back with the size and alignment it
  was allocated with. The real-time guard watches the allocation entry point too.
- On x86-64, `irama::RenderScope` sets flush-to-zero and denormals-are-zero while it is open and
  restores the thread's floating-point environment when it closes, so scopes nest.
- `irama::Player`, created from a `PlayerConfig` with its voice pool and working buffer from the
  config's allocator. Note-on, note-off, release-all and set-parameter messages are queued at
  absolute frames, and rendering splits each 256-frame block at their frames, so the output is the
  same however the render calls are split.
- `irama::ParamDescriptor` and `irama::ParamId`, conversions between plain and normalised values,
  and `irama::ParamSmoother`, a linear ramp in the normalised domain that lasts exactly the
  descriptor's smoothing time. `irama::instrument_params` describes every parameter of the basic
  synth. `Player::set_parameter()` clamps the value and rounds a stepped one; at its frame, the
  value glides while a voice sounds and jumps while none does.
- The basic synth's voice: an oscillator playing a sine, a triangle, a saw or a pulse with a pulse
  width from 5 to 95 %, band-limited with 2-point PolyBLEP and polyBLAMP; white noise from
  `irama::Pcg32`; coarse and fine tuning in twelve-tone equal temperament with A-4 at 440 Hz; and
  an ADSR envelope whose segments last exactly their time and bend with a curve for each segment.
  The amplifier has gain in decibels, constant-power pan and velocity sensitivity.
- `irama::Svf`, a zero-delay-feedback state variable filter with low-pass, high-pass, band-pass
  and notch modes at 12 and 24 dB per octave, between the oscillator and the envelope. Changing the
  waveform, the filter mode or the slope during a note crossfades over 5 ms.
- Voice stealing. A note-on with every voice sounding takes the oldest voice in its release, else
  the quietest, else the oldest, and the stolen voice fades out over 5 ms in one of 8 fade slots,
  so the new note still starts on its own frame.

### irama-cli

#### Added

- `irama-cli synth`, which plays a script of `at <frame> <command>` lines (`note-on`, `note-off`,
  `release-all` and `set-parameter`) through a player into a WAV file. It checks the whole script
  first and names the line of anything it refuses, with status 2.

### Irama Studio

#### Added

- Irama Studio, built when `IRAMA_BUILD_STUDIO` is on, on Linux: a window drawn with Dear ImGui
  through SDL_GPU, with docking, File, View and Help menus, an About window, and the Inter font.
- `studio.ini` and `imgui.ini` in `$XDG_CONFIG_HOME/irama/`, or another directory given with
  `--config-dir`, which remember the window's size, the font size, the layout, the audio device,
  the buffer size and the keyboard octave.
- Sound through miniaudio, on PulseAudio or ALSA. The computer keyboard plays notes in the tracker
  layout, with numpad `/` and `*` stepping the keyboard octave. The status bar shows the octave,
  the voice count, the audio callback's CPU peak and a clip indicator.
- The Audio window, which chooses the device and a buffer size of 128 to 1,024 frames, and reopens
  on the system default when a named device is lost.
- The Instrument window, which shows a control for every synth parameter, built from its
  descriptor and grouped by section, with values written in their units and typed values read
  back.

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

[0.2.0]: https://github.com/takumu-dev/irama/tree/0.2.0
[0.1.0]: https://github.com/takumu-dev/irama/tree/0.1.0
