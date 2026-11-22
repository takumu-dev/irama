// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

/// \file
/// The band-limited oscillator.

#pragma once

#include <irama/engine/base/nonblocking.hpp>

#include <cstdint>
#include <span>

namespace irama {

/// The waveforms an `Oscillator` plays, in the order of the waveform parameter's values.
enum class Waveform : std::uint8_t {
    /// `sin(2π·t)`.
    sine,
    /// Rising from 0 to +1 over the first quarter of the period, falling to −1 at three quarters,
    /// and rising back to 0.
    triangle,
    /// Rising from 0 to +1 over the first half of the period, then jumping to −1 and rising back
    /// to 0.
    saw,
    /// +1 for the first part of the period, as wide as the pulse width, and −1 for the rest.
    pulse,
};

/// An oscillator playing a sine, a triangle, a saw or a pulse, band-limited so that it aliases
/// little up to about 2 kHz.
///
/// Every waveform reads one phase, a `float` that counts cycles in [0, 1), starting at 0. Each
/// sample it moves on by the frequency over the sample rate, computed in `float`, and wraps by
/// subtracting 1, so it keeps the same precision however long the oscillator runs, and a change of
/// frequency or waveform keeps it continuous. Each waveform starts at 0, or at +1 for the pulse,
/// and its fundamental is in phase with the sine's.
///
/// **Aliasing.** A naive saw or pulse jumps between two samples, and a naive triangle turns a
/// corner, and the harmonics those make above half the sample rate fold back below it as
/// aliasing. The saw and the pulse correct each jump with 2-point PolyBLEP: the sample on each side
/// of a jump of height `h` adds `h/2` times the residual of a band-limited step, a polynomial in
/// how far the sample is from the jump, `x` in [−1, 1] increments: `(1 + x)² / 2` before it and
/// `−(1 − x)² / 2` after it. The triangle corrects each corner with 2-point polyBLAMP, the
/// integral of the same residual: a corner where the slope changes by `s` per sample adds
/// `s · (1 − |x|)³ / 6` to the sample on each side, so the triangle needs no integrator and has no
/// DC offset. The sine needs no correction.
///
/// **Limits.** The test measures the strongest component that isn't a harmonic, relative to the
/// fundamental, with a 65,536-point Blackman-Harris-windowed FFT at 48 kHz, for the saw, the pulse
/// at 50 % and 10 % and the triangle at 110, 440 and 1,760 Hz. Each must stay within its first
/// measurement plus 3 dB, and at least 20 dB below the naive waveform's. 2-point PolyBLEP is
/// alias-free by the perceptual measure to about 2 kHz. Above that a saw or a pulse aliases
/// audibly, and the higher the note the more. A higher-order correction would reach further at
/// more cost for each period.
///
/// **Pulse width.** The width is latched at the start of each period, at the phase wrap, so it
/// never changes inside a period, and the falling edge has the same width on both of its samples.
///
/// **Frequency.** A frequency above 0.45 of the sample rate, which a high note tuned up can reach,
/// is held there, so that the phase always moves by less than half a cycle a sample, as the
/// corrections assume.
class Oscillator {
  public:
    /// The highest frequency an oscillator plays, as a fraction of the sample rate.
    static constexpr float max_frequency_ratio = 0.45F;

    /// Starts the oscillator again from phase 0, at the start of a period.
    ///
    /// \param sample_rate The sample rate in hertz, above 0, which is checked.
    void start(float sample_rate);

    /// Writes the next samples of a waveform, one for each element of `out`, and moves on by as
    /// many.
    ///
    /// \param out Where to write the samples, overwriting what it holds.
    /// \param waveform Which waveform to play.
    /// \param frequencies The frequency of each sample in hertz, above 0, at least as many.
    /// \param widths The pulse width of each sample, from 0.05 to 0.95 of the period, at least as
    ///               many. Each period latches the width of its first sample, whatever the
    ///               waveform, so a change to the pulse plays the width latched last.
    void render(std::span<float> out, Waveform waveform, std::span<const float> frequencies,
                std::span<const float> widths) IRAMA_NONBLOCKING;

  private:
    float sample_rate_ = 0.0F;
    float phase_ = 0.0F;
    // The pulse width latched for this period.
    float width_ = 0.5F;
    // Whether the next sample starts a period, and so latches its width.
    bool period_starts_ = true;
};

} // namespace irama
