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
| `base` | Assertions, `irama::narrow`, the `IRAMA_NONBLOCKING` mark, the allocator with the owning pointer and array, the render scope with the real-time guard and the floating-point environment, the single-producer, single-consumer queue, and the PCG32 random number generator. | nothing |
| `capi` | The C API: `irama.h` and its implementation. | `base` |
| `wav` | The WAV writer, which irama-cli and the tests share. | `base` |
| `param` | Parameters: the descriptor that says what each one is, its scale conversions, and the smoother that glides it to each new value. | `base` |
| `source` | The basic source a voice plays, the band-limited oscillator and the noise, and the tuning that gives each note its frequency. | `base` |
| `filter` | The state variable filter a voice plays its source through. | `base` |
| `voice` | A voice, one sounding note, the ADSR envelope that shapes it, how a player chooses and steals voices, and the instrument: its parameter table, and the values every voice reads from it. | `source`, `filter`, `param`, `base` |
| `player` | `irama::Player`, the object a host renders, which owns the instrument, the voices and the queue of control messages. | `voice`, `param`, `base` |

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

## The player

`irama::Player`, in `<irama/engine/player/player.hpp>`, is the object a host renders.
`Player::create()` takes a `PlayerConfig`: the sample rate, which is fixed for the player's life,
the number of voices, 32 by default and at most 256, which `IRAMA_ASSERT` checks, the seed of its random numbers, and the
allocator. It allocates the player, its
voices, its fade slots and its working buffers there and then, and never again, and returns them in a `PlayerPtr`,
the owning pointer, or `CreateError::out_of_memory`, having freed whatever it had allocated. The
player never moves, so the pointer the audio thread holds stays valid. A player is created and
destroyed on a control thread and rendered on the audio thread, never both at once.

- **Control messages.** `note_on()`, `note_off()`, `release_all()` and `set_parameter()` are
  called from one control thread. Each checks its arguments with `IRAMA_ASSERT`, so a bad note,
  velocity or parameter never reaches the audio thread, and queues a control message in the player's own `irama::SpscQueue` of 1,024. Each
  returns whether the message was queued: a full queue refuses it and changes nothing. A message
  names an absolute frame, counted from the first frame the player renders, and frames never go
  backwards from one message to the next, which `IRAMA_DEBUG_ASSERT` checks. Notes are numbered as
  the note column numbers them, from C-0 at 0 to B-9 at 119, and a velocity runs from 0 to 1.
- **The render call.** `Player::render()` fills a span of interleaved stereo frames, any even
  length, none included. It is the only libirama function that opens a render scope, and it
  carries `IRAMA_NONBLOCKING`, as each component's per-block render function does. It renders in
  blocks of 256 frames, counted from the player's first frame, and splits a block at the frame of
  each message, so each message applies exactly at its frame, and the output is the same however
  the host splits its calls. A message whose frame has already been rendered applies at the first
  frame of the next render call. The queue has no peek, so the player keeps the one message it has
  popped whose frame hasn't come yet, and one render call pops at most 1,024 messages. The player
  writes what its voices sum to, unclipped.
- **Voices.** A note-on starts the first free voice. With none free, it steals one, as
  `irama::choose_voice()` chooses: the oldest voice in its release, or with none releasing the
  quietest, whose envelope level times its velocity's gain is lowest at the note-on's frame, and of
  the equally quiet the oldest. A voice's age is the order its note started in, however long it has
  been releasing. The stolen voice is copied into one of 8 fade slots, allocated beside the voices,
  and fades out there linearly over 5 ms, rounded to whole frames: frame `i` of `n` is at
  `1 − (i + 1) / n` of its level, so its last frame is silent. Its own voice starts the new note on
  the note-on's frame. With all 8 fade slots busy, `irama::choose_fade_slot()` cuts the fade nearest
  its end. A note-off releases every held voice playing its note, and a release-all every held
  voice. The player keeps one `irama::Pcg32`, of the config's seed on stream 0, a fixed number by default,
  and each note-on splits the new voice's noise generator from it: a seed and a stream drawn from
  its next four outputs. So voices' noise is uncorrelated, and the output depends only on the seed
  and the control messages. After
  each render call, the number of voices sounding is written to an atomic, which `active_voices()`
  reads on any thread. Fading voices don't count, so it never exceeds `max_voices`.
- **Parameters.** The player owns one `irama::Instrument`, the live value of each of the
  instrument's parameters, filled with their defaults when it is created. `set_parameter()` takes
  a parameter's ID and a plain value. On the control thread it finds the parameter's index in
  `irama::instrument_params`, asserts that the ID is one of them and that the value isn't NaN, and
  constrains the value, clamping it to the range and rounding a stepped value to the nearest whole
  number, so a slider's 1.0001 is never an error. At its frame the value goes to the parameter's
  smoother: a parameter with a smoothing time glides to it if any voice is sounding, a fading one
  included, and jumps to it if none is. So a new player given its values by `set_parameter()` at frame 0 renders as if it had
  been created with them, which is how a host replaces a player and keeps its values.

### Parameters

The `param` component describes parameters, and knows nothing of any instrument. An
`irama::ParamDescriptor` gives a parameter's `irama::ParamId`, a strong type over `std::uint32_t`;
its name, unit and display precision; its `min`, `max` and default; its scale, linear, logarithmic
or stepped; a label for each step, where a stepped parameter has them; its smoothing time; and
whether it is automatable and modulatable. An instrument's descriptors are a `constexpr`
`std::array` with static storage, and every ID is written out in it. `irama::find_param()` finds a
parameter's index by its ID, and `irama::find_table_fault()` checks a table: unique IDs, a minimum
below its maximum, a default within the range, a logarithmic minimum above 0, whole-number bounds and
defaults for stepped parameters, one label for each step where there are labels, and no smoothing
time below 0 or on a stepped parameter. A test runs it on every table.

Values are **plain**, in the parameter's own unit, percentages from 0 to 100 among them, everywhere
but at the edges. `to_normalized()` and `from_normalized()` map them to and from 0–1 for a slider:
`(v − min) / (max − min)` for a linear or stepped parameter, and `log(v / min) / log(max / min)` for
a logarithmic one.

`irama::ParamSmoother` holds one parameter's live value. A smoothed set starts a linear ramp in the
normalised domain, so a cutoff glides evenly in octaves, lasting exactly the smoothing time in whole
frames. Its `i`th frame of `n` is at progress `(i + 1) / n`, so its last frame is at the new value
exactly, and a test can say when it arrives. A new value during a ramp starts a full ramp from the
current value. A parameter with no smoothing time, every stepped one among them, switches at its
frame. The smoother writes a block's values without moving on, and moves on separately, so the
instrument can render several parameters for one block.

### The instrument and its voices

`voice` declares the instrument's table, `irama::instrument_params`, and the IDs in
`irama::InstrumentParam`. IDs fall in decimal blocks: the filter at 1000, the amplifier at 2000,
the amplifier's envelope at 3000, and the subtractive source at 10000, whose oscillator `n` starts at
`10000 + 10 · (n − 1)` and whose noise is at 10090. The table holds every parameter of the basic
synth. Continuous parameters heard while a note sounds glide over 20 ms. Stepped parameters, and those read
only when a note or a segment starts, such as the envelope's times and curves and the velocity
sensitivity, switch at their frame.

`irama::Instrument` holds a smoother for each parameter, at the same index as in the table, and
advances them every frame, whether a voice reads them or not, so there is one ramp for each
parameter, shared by every voice. For each block it computes what every voice reads, once: the
sustain level of each frame, the gain of each frame on each channel, the source's values: the
waveform, which holds for the block, and the oscillator's level, the pulse width, the noise's level
and the tuning of each frame, the ratio `2^((coarse + fine / 100) / 12)`; and the filter's: its mode
and slope, which hold for the block, and its coefficients of each frame, `g = tan(π·fc/fs)` and the
values derived from it, computed from the smoothed cutoff and resonance. The gain is in decibels,
and the pan is constant-power, `sin(π/2 − θ)` on the left and `sin θ` on the right, with θ from 0 at
−1 to π/2 at +1, so a centred voice is 3 dB down on each side, the same on both to the bit.

`irama::Voice` plays the basic source through an `irama::Svf`, shaped by an `irama::Adsr`, through
the amplifier: an `irama::Oscillator` at the note's frequency from `irama::note_frequency()` times
the tuning, at the oscillator's level, plus `irama::Noise` at the noise's level. It reads the
instrument as it is when it needs a value: the velocity's gain, `1 − s · (1 − v²)` for the
sensitivity `s`, the waveform and the filter's mode and slope when it starts; each segment's time
and curve when that segment starts; and the source's values, the filter's coefficients, the sustain
level and the channels' gains every frame. It clears its filter's state when it starts. The tuning
is twelve-tone equal temperament with A-4, note 57, at 440 Hz. A change of waveform, filter mode or
filter slope while a voice sounds crossfades linearly over 5 ms, rounded to whole frames: the voice
copies its oscillator and its filter, plays the old configuration on the copies and the new one on
its own, from the same phase and the same filter state, and frame `i` of `n` is `(i + 1) / n` of
the new one. Both hear the same noise. A change during a crossfade starts a new one from the
configuration fading in. A coarse step is a jump in pitch with a continuous phase, and doesn't
crossfade. A voice that `fade_out()` starts fading, as a stolen one does, multiplies its envelope's
levels by the fade's, and is free on the frame after the fade's last, or after its release ends, if
that comes first.

The oscillator plays a sine, a triangle, a saw or a pulse from one `float` phase in [0, 1), which
moves on by the frequency over the sample rate each sample. Each waveform's fundamental is in phase
with the sine's. The saw and the pulse correct each jump with 2-point PolyBLEP, and the triangle
corrects each corner with 2-point polyBLAMP, so it needs no integrator and has no DC offset. The
pulse width runs from 5 % to 95 % and is latched at each phase wrap, so it never changes inside a
period. A frequency above 0.45 of the sample rate is held there. 2-point PolyBLEP is alias-free by
the perceptual measure to about 2 kHz: it all but removes the aliases that fold down from near the
sample rate, but weakens those just below half the sample rate by only about 8 dB. The oscillator's
test measures the strongest component up to 8 kHz that isn't a harmonic, relative to the
fundamental, for the saw, the pulse at 50 % and 10 % and the triangle at 110, 440 and 1,760 Hz,
with a 65,536-point seven-term Blackman-Harris-windowed FFT. Each limit is the first measurement
plus 3 dB, and is at least 20 dB below the naive waveform's at the same note. The noise is uniform,
each sample the top 24 bits of its generator's next output, scaled to [−1, 1).

The filter is Zavalishin's linear state variable filter in the topology-preserving transform, with
trapezoidal integrators, so its response is the analogue one's under the bilinear transform
prewarped at the cutoff. It has low-pass, high-pass, band-pass and notch modes, the band-pass
normalised to a gain of 1 at the cutoff, and slopes of 12 and 24 dB per octave. The 24 dB slope is
two stages in series in the same mode at the same cutoff: the first Butterworth-damped, with Q =
1/√2, and only the second carrying the resonance, so there is one resonant peak. The 12 dB slope is
the resonant stage alone, and the other stage holds no state while it plays. The resonance `r`, from
0 to 1, gives `Q = 0.5 · 50^r`, from 0.5 to 25, so the filter never self-oscillates, and the cutoff
is held at most at 0.45 of the sample rate. The default sound is the low-pass at 12 dB, open at
20 kHz with no resonance. At the end of each block the filter sets any state below 1e-20 in
magnitude to 0, on every target and whatever the floating-point environment, so a filter ringing
into silence settles to exact zeros rather than denormals. The filter's test checks every mode at
both slopes, at cutoffs from 100 Hz to 15 kHz and resonances of 0 and 1, against the analytic
response within 0.1 dB below 0.4 of the sample rate, wherever it is above −60 dB, from the FFT of
the impulse response.

Each segment of the ADSR lasts exactly its time, rounded to a whole number of frames. Its `i`th
frame, counting from 0, of `n` is at progress `p = (i + 1) / n`, and moves from the segment's start
level to its end level as `start + (end − start) · shape(p)`, where `shape(p) = (1 − e^(−k·p)) /
(1 − e^(−k))` with `k = 6 · curve`, or `p` for a curve of 0. So the last frame of a segment is at its
end level exactly. The decay heads for the sustain level as it is at each frame, and the sustain
follows it. A release starts from the current level, mid-attack included, and a voice is free again
on the frame after its release ends, with no threshold.

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

`irama-cli synth <file.wav> --script <file> [--seconds <seconds>] [--rate <hertz>]` plays a script
through a player, for one second by default, at 48 kHz unless `--rate` gives 44100, 48000, 88200 or
96000. A script has one control message on each line, at an absolute frame, with `#` beginning a
comment:

```
at 0      note-on C-4 0.8          # a note as the note column writes it, and a velocity from 0 to 1
at 12000  set-parameter 2001 -0.5  # a parameter's ID, and its plain value
at 24000  note-off C-4
at 36000  release-all
```

irama-cli reads and checks the whole script before the player sees any of it, so no script can
break one of the player's preconditions. A malformed line, a note or a velocity out of range, a
parameter ID the instrument doesn't have, a parameter value that isn't a finite number, or a frame
before the one on an earlier line exits with status 2, naming the line. A parameter value out of
its range is clamped by the player, as any host's would be. It renders 4,096
frames at a time, queuing each message before the render that reaches its frame.

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

The synth's goldens play scripts from `tests/golden/scripts/` through `irama-cli synth`, at the
player's default seed. `synth-waveforms` plays one note through the sine, the triangle, the saw and
the pulse, a tenth of a second each, so it records the crossfades between them, then the noise
alone, and the release. `synth-filter` plays a saw through each filter mode at both slopes, with
resonance and a cutoff sweeping up and back down, so it records the crossfades between modes and
slopes too. `synth-envelope` holds one note at its sustain, then releases it to the exact frame its
release ends. `synth-chord` plays four overlapping notes of the default sound at different
velocities.

`irama_test_support`, in `tests/support/`, is what the tests measure audio with: a radix-2 FFT in
double precision, checked against a direct DFT, the seven-term Blackman-Harris window, and the
strongest component of a periodic signal that isn't one of its harmonics, up to a frequency. It
links only the standard library, so a component's test program can link it and still link no other
part of libirama.

## Irama Studio

Studio opens one window, `Irama Studio`, 1280×800 logical pixels the first time it opens and
resizable, and draws [Dear ImGui](https://github.com/ocornut/imgui) in it through SDL3's GPU API,
SDL_GPU, which runs on Vulkan on Linux and on Direct3D 12 on Windows. So running Studio on Linux
needs a Vulkan driver. Studio offers SDL_GPU its shaders as SPIR-V, DXIL, DXBC and Metallib; ImGui's
Direct3D 12 shaders are DXBC. On Windows Studio is a console program, so stderr and SDL's log behave
as on Linux.

- **The loop.** Studio has its own `main()` and a plain loop: handle SDL's events, draw a frame, and
  present it with vsync, so it redraws every vsync while the window can be seen. While the window is
  minimised or hidden, the loop waits for the next event instead, for 250 ms at most, so that it
  still looks at the audio device's heartbeat.
- **Docking.** Windows dock inside the main window only, in a dock space that fills it between the
  main menu bar and the status bar. ImGui's windows of their own, outside the main window, are off:
  they work only under some of SDL's video drivers, and never on native Wayland.
- **The default layout**: File (Quit), View (one toggle for each window), Keyboard (Lower and
  Raise keyboard octave) and Help (About) in the main menu bar; the Instrument window docked on the left, at two thirds of the width, and the Audio
  window on the right; and the status bar along the bottom, with the keyboard octave between a −
  and a + button, the voice count, the CPU meter, and a red CLIP for one second after the last
  clipped sample. Studio builds the layout only when there is no saved one.
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
- **Settings.** Studio keeps two files in its settings directory. On Linux that is
  `$XDG_CONFIG_HOME/irama/`, or `~/.config/irama/` where `XDG_CONFIG_HOME` isn't set or isn't an
  absolute path. On Windows it is `irama` in the Roaming application data folder, read from
  `APPDATA`. On either, `--config-dir <directory>` gives another. With none of those variables set
  and no option, Studio keeps nothing.
  - `studio.ini` holds `key = value` lines, which Studio reads and writes itself: `window_width`
    and `window_height`, the window's size when it isn't maximised, in logical pixels (at least
    640×400, the window's minimum); `window_maximised` (`true` or `false`); and `font_size`, the UI
    font's size in logical pixels, from 10 to 24, 15 by default; `audio_device`, the audio
    device's name, or empty for the system default; `buffer_size`, the frames in each audio
    callback, 128, 256, 512 or 1024, 256 by default; and `octave`, the keyboard octave, from 0 to
    8, 4 by default. A key Studio doesn't know is ignored, and a value it can't read, or one out of its range, leaves that setting at its default
    and the others as read. There is no version key. The window's position isn't kept, since
    Wayland doesn't let a client place its window.
  - `imgui.ini` holds ImGui's layout: where the windows are docked, and their sizes. ImGui hands it
    over as text, through its in-memory load and save functions, with `io.IniFilename` null, so it
    never writes to the working directory.
  - A file that is missing or can't be read gives the defaults, never an error. Each file is written
    one second after its last change, so that a drag writes it once, and again on quit if it has
    changed since. Each write goes to a temporary file beside it, then renames that over it, so the
    file always holds a whole version. A file that can't be written is reported on stderr, and
    Studio carries on.
- **Start-up failure.** If a step of start-up fails, such as SDL's video, the window, the GPU
  device or claiming the window for it, Studio prints the step and SDL's error, or "SDL gave no
  reason" where SDL has none, to stderr, shows the same in a message box, and exits with status 1.
  Under `--exit-after-frames` there is no message box, which would wait for a click in a test.

### Sound and the note keys

Studio plays through two layers in `studio/src/audio/`:

- **`AudioDevice`** wraps [miniaudio](https://github.com/mackron/miniaudio) and knows nothing of
  Irama. It owns one `ma_context`, started on the first backend in its list that works, and at most
  one `ma_device` on it, which plays interleaved stereo 32-bit float frames from a
  `void(std::span<float>)` callback. It lists the playback devices by name, and opens one by its
  name, or the system default, with a null device ID, which follows the user's choice of default.
  Opening a device reports what it was granted: the backend, the
  rate, the number of frames in each callback and the device's name, or the error that stopped it.
  The callback runs only once the device is started, so the caller can first prepare for the rate.
  Closing is always `ma_device_stop()` then `ma_device_uninit()`, since uninitialising a device that
  has lost its server otherwise waits forever. The context asks for real-time priority for the
  device's thread, which miniaudio lowers to normal where the system refuses it. miniaudio's log
  never reaches stderr: the device keeps its latest error, from any thread, for the Audio window,
  and drops everything else, since after an ALSA device is lost, warnings come hundreds of
  thousands of times a second. miniaudio's
  implementation compiles in Studio's own `miniaudio.cpp`, with each preset's flags, so the
  sanitizers instrument its threads too.
- **`AudioEngine`** holds the device and, only while it is open, an `irama::Player` with 32 voices
  at the device's rate. Studio's backends are PulseAudio, then ALSA, on Linux, and WASAPI alone, in
  shared mode, on Windows. It opens a device at the device's own rate, and if that isn't one of the
  player's, 44.1, 48, 88.2 or 96 kHz, opens it again at 48 kHz for the backend to convert. Each callback has exactly the buffer size's frames,
  and miniaudio's own clipping is off. With no backend or device working, Studio runs silent, and
  the Audio window says why.

The engine opens the device that `audio_device` names, where it is listed, and otherwise the system
default, with a note that the named device wasn't found. The setting is kept either way, so the
device opens again by name once it is back. Every open, whether of another device, at another
buffer size, or by Reopen, closes the device, drops the player and creates a new one, so the held
keys are forgotten. Listing and opening run on the UI thread, and a frame may drop while they do.

No Linux backend reports a lost device: a stream on a named device is orphaned when the device is
unplugged, while one on the system default moves to the next device. So the UI thread looks at the
callback's heartbeat each frame, and a device whose heartbeat hasn't moved for a second is lost and
closed. A named device then gives way to the system default, with a note that it was lost. A lost
system default, or a fallback that fails, leaves Studio silent, with the error and a Reopen button.
Nothing is tried again in the background.

The Audio window shows the backend; a Device combo, System default and then the devices by name,
listed again each time it opens; a Buffer size combo, in frames and in milliseconds at the
device's rate; what the device plays at, its rate and frames a callback, or that Studio is silent;
the latest error or note, and miniaudio's latest error; and Reopen while silent.

The audio callback carries `IRAMA_NONBLOCKING` and opens its own `irama::RenderScope`, which the
player's scope nests in, so the real-time checks watch Studio's own work in the callback as well as
the render. It renders the player, clips each sample to [−1, 1], and reports to the UI thread
through relaxed atomics only: the peak of its run time over its period (the frames it rendered
over the rate), timed with `std::chrono::steady_clock`; whether it clipped; the peak magnitude it
played; and a heartbeat, a count of its runs. The UI thread takes the CPU peak every half second,
resetting it, and shows it as a percentage.

The note keys, in `studio/src/input/`, include no SDL header: they take USB HID usage IDs, which
SDL's scancodes are, so the piano rows are the same keys on every keyboard layout. At keyboard
octave O, the lower row `Z S X D C V G B H N J M` plays C to B of O and `, L . ; /` C to E of O+1,
and the upper row `Q 2 W 3 E R 5 T 6 Y 7 U` plays C to B of O+1 and `I 9 O 0 P` C to E of O+2, as
a US keyboard labels the keys. The octave runs from 0 to 8, 4 by default, so `Z` plays C-4, and a
key whose note would pass B-9 plays nothing. Numpad `/` lowers it and Numpad `*` raises it, as
shortcuts.

- Every note key plays at velocity 1, and key repeats are ignored.
- A key's note is remembered when it goes down, so a key released after an octave change ends the
  note it started. A note-off releases every voice on its note, so the held keys are counted for
  each note, and only the release of the last key holding a note sends its note-off.
- A key going down plays nothing while ImGui takes typed text, while it runs a shortcut, or while
  Ctrl, Alt or Super is held. A key going up always ends its note. When the window loses focus,
  Studio sends release-all and forgets the held keys.
- Studio's messages name frame 0, so each applies at the start of the player's next render call.
  A message the player's queue refuses waits in a backlog on the UI thread, and the backlog is sent
  first, in order, before any new message and again each frame. With no device open there is no
  player, and the note keys do nothing.

So Studio runs three kinds of thread: the UI thread, which handles events, draws, and makes every
call on the device, the engine and the player's control functions; the device's thread, miniaudio's
own, which runs the callback; and the backend's threads, such as PulseAudio's, which Studio never
touches.

### Shortcuts and the key router

Every Studio key that runs a named function is a shortcut, an entry in one table in
`studio/src/input/shortcut_table.cpp`. An entry holds an ID, the name the menus show, a context and
one or more chords. The menus show each entry's chords from the table, and choosing an entry runs the
same function as its chords, through `run_shortcut()`. Like the note keys, the table includes no
SDL or ImGui header.

- **Chords.** A chord is a named key, or the character a key types in the current layout, with
  Ctrl, Alt and Shift, each meaning either side's key. A named key is one whose place and meaning
  are the same on every layout, such as Space, the arrows, the F-keys and the numpad, and is its USB
  HID usage ID. A character is matched in the current layout, so Ctrl+Z is on whichever key types
  Z, while the note keys stay where they are: SDL gives the key's character with no modifier, and
  gives a layout with no Latin letters the US layout's. Super is in no chord.
- **Contexts.** A global shortcut works whichever window has focus. A Pattern or Sequence shortcut
  works only while that window has focus. Shortcuts whose contexts overlap, a global one with any
  other or two in one context, never share a chord.
- **Unmodified chords.** Plain letters, digits and punctuation belong to the note keys and to
  entry, so a chord with no modifier may use only Space, Enter, Esc, Tab, the arrows, Page Up and
  Page Down, Home and End, Insert, Delete and Backspace, the F-keys, the numpad, Scroll Lock and the
  lone modifiers. No unmodified chord sits on a note key or a character the note keys type.
- **Lone modifiers.** Right Ctrl and Right Alt may each be a chord alone, the only chords that name
  a side. Such a shortcut runs when the key goes up, and only if no other key was held when it went
  down and no other key or mouse button went down while it was held. So Right Ctrl+Z is Ctrl+Z
  alone, and Right Alt works as a lone key on layouts where it is AltGr.

One router, `KeyRouter`, sends each key where it belongs, and the first rule that applies wins:

1. While a text field has focus (ImGui's `WantTextInput`), it takes every key.
2. A key that matches a chord of a shortcut in force runs that shortcut. A held key's repeats run
   nothing.
3. Otherwise the key goes to the note keys.

A key going up always reaches the note keys, so a held note key ends its note whatever was pressed
while it was held. Tests check the table itself: that every chord is one Studio allows, that no
unmodified chord sits on a note key, and that no two shortcuts whose contexts overlap share a chord.

### The Instrument window

The Instrument window shows every parameter in `irama::instrument_params`, built from the
descriptors rather than a list of its own, so a parameter added to the table shows up without a
change to the window. Each parameter goes under one collapsing header by the block its ID falls in:
Oscillator for the source's blocks, from 10000, then Filter (the 1000s), Amplifier (the 2000s) and
Envelope (the 3000s), with any ID outside them under a last header, Other.

- A linear or logarithmic parameter has a slider that moves in normalised values, through the
  descriptor's `to_normalized()` and `from_normalized()`, so a logarithmic one moves evenly in
  octaves. A stepped parameter with labels has a combo box of them, and one without a slider over
  whole numbers.
- A slider shows the plain value as text, written by Studio: the number to the descriptor's
  precision and its unit's symbol (`440 Hz`, `-12.0 dB`, `0.300 s`, `50.0%`, `+7 st`, `-5 ct`),
  or a stepped parameter's label. Semitones and cents above 0 take a `+`, and nothing is written
  `-0`.
- Double-clicking a slider sets the parameter's default. Ctrl-clicking one puts a text field in its
  place, which takes a number, with or without the unit's symbol, or a label in any case. Enter sets
  the value, constrained to the parameter, and Escape or a click elsewhere leaves it as it was.

`AudioEngine` keeps Studio's own copy of the values, by ID, which starts from the defaults at every
run, since the values are song data rather than settings. The window always shows the copy, and
each change sets it and sends set-parameter at frame 0 at once, so a drag sends at most once a
frame. A value the player's queue refuses stays marked in the copy and goes again with the next
frame's flush, at its latest value only. Every new player is given the whole copy by set-parameter
at frame 0 before its device starts, so it holds every value at its first frame. With no device
open, a change only updates the copy.

`irama_studio_core` is laid out in folders, which are not targets of their own: `app/` (the loop,
the window, the command line, the settings), `audio/` (the device, the engine, its meters and
Studio's copy of the instrument's values), `input/` (the note keys, the shortcut table and the key router), `ui/` (the theme, the font, the
values' text and helpers) and `views/` (what the window shows). `irama-studio` is `main.cpp` alone.
Studio's tests are one Catch2 program, `irama_studio_tests` in `tests/studio/`, which tests what
needs no window. One of its tests checks that SDL was built with its `wayland`, `x11` and
`offscreen` video drivers on Linux, and its `windows` driver on Windows, since SDL quietly leaves
out a driver whose development packages are missing. The play test opens the engine on miniaudio's Null backend, a device with no hardware that
a timer drives on miniaudio's own thread, plays a chord through the note keys and lets the real
callback render it: so the real-time guard and every sanitizer watch Studio's callback while it
plays.

`irama-studio --exit-after-frames <count>` quits after drawing that many frames. The smoke test,
`cmake/smoke_test.cmake`, runs it for 300 frames, with `--config-dir` giving an empty directory in
the build tree, so it never reads or changes the user's settings. On Linux it runs under SDL's
`offscreen` video driver, with no display, and SDL_GPU draws on the machine's own Vulkan driver, and
on lavapipe, Mesa's Vulkan driver on the CPU, in CI. On Windows the `offscreen` driver's windows
have nothing Direct3D 12 can present to, so Studio opens a real window under SDL's `windows` driver,
drawn by WARP, Direct3D 12 on the CPU, in CI. It passes only if Studio exits with status 0, writes
nothing to stderr, and leaves `studio.ini` and `imgui.ini` in that directory. It passes whether or
not a sound device opens, since Studio runs silent without one, as it does in CI, which has no sound
device. The test carries the `smoke` label, and runs in every test preset that builds Studio but
`clang-tsan`: SDL3 and the Vulkan driver aren't built with ThreadSanitizer, so their threads would
give false reports.

## Dependencies

Every dependency comes from `vcpkg.json`. libirama's are its top-level dependencies, and each other
part's are a manifest feature. Studio's are SDL3, with its default features off and `vulkan`,
`wayland` and `x11` on Linux, where Vulkan is SDL_GPU's only driver, while on Windows SDL_GPU's
Direct3D 12 driver needs no feature; Dear ImGui's docking branch, with its SDL3 and SDL_GPU backends;
miniaudio; and Inter. The overlay ports in `ports/`, which `vcpkg-configuration.json` puts ahead of vcpkg's own,
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
| `cmake/smoke_test.cmake` | Starts Studio, headless on Linux and in a window on Windows, and checks that it quits cleanly. |
| `cmake/embed_file.cmake` | Writes a file's bytes into a C++ source, for Studio's font. |
| `studio/` | Irama Studio: `irama_studio_core`, `irama-studio`, and the `.clang-tidy` that lets ImGui's printf-style functions through. |
| `tests/` | The tests, one folder per component under `unit/`, the programs that must abort under `abort/`, the golden cases and the compare tool under `golden/`, Studio's tests under `studio/`, `irama_test_support` under `support/`, and the CLI and configuration checks. |
| `ci/` | The CI's Linux image, under `linux/`, the Windows job's scripts, under `windows/`, and the scripts the Linux jobs share. |
| `THIRD_PARTY_NOTICES.md` | The notices of the libraries the applications ship. |
| `REUSE.toml`, `LICENSES/` | The licences of files that can't hold a comment, and their texts. |
| `.gitattributes` | Marks WAV files binary. |
