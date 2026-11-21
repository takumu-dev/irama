// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

/// \file
/// One voice: a note sounding from its note-on to the end of its release.

#pragma once

#include <irama/engine/base/nonblocking.hpp>
#include <irama/engine/base/pcg32.hpp>
#include <irama/engine/source/noise.hpp>
#include <irama/engine/source/oscillator.hpp>
#include <irama/engine/voice/adsr.hpp>
#include <irama/engine/voice/instrument.hpp>

#include <cstddef>
#include <cstdint>
#include <span>

namespace irama {

/// One note, sounding from its note-on until its release ends.
///
/// A voice is the basic source, an `Oscillator` at the note's frequency tuned by the coarse and
/// fine offsets, at the oscillator's level, plus `Noise` at the noise's level, shaped by an `Adsr`,
/// through the amplifier: the instrument's gain and pan on each channel, and the gain of the note's
/// velocity. It reads the instrument's values as they are when it needs them: the velocity's gain,
/// the waveform and the attack's settings when it starts, each later segment's settings when that
/// segment starts, and the source's values, the sustain level and the amplifier's gains every
/// frame. A voice is free until it starts, and free again on the frame after its release ends.
///
/// A change of waveform while the voice sounds crossfades linearly over 5 ms, rounded to the
/// nearest whole number of frames: the voice plays the old waveform and the new one from the same
/// phase, and frame `i`, counting from 0, of `n` is `(i + 1) / n` of the new one, so the last frame
/// is the new waveform alone. A change during a crossfade starts a new one, from the waveform that
/// was fading in. A coarse step is a jump in pitch with a continuous phase, and doesn't crossfade.
class Voice {
  public:
    /// How many floats of scratch `render()` needs for each frame.
    static constexpr std::size_t scratch_per_frame = 3;

    /// How long a crossfade between waveforms lasts, in seconds.
    static constexpr float crossfade_seconds = 0.005F;

    /// Starts the voice playing a note, from the start of its attack.
    ///
    /// \param note The note, from `lowest_note` to `highest_note`, which is checked.
    /// \param velocity How hard the note is played, from 0 to 1.
    /// \param instrument The instrument, as it is at the note's frame.
    /// \param noise The generator the voice's noise draws from, which it keeps its own copy of.
    void start(int note, float velocity, const Instrument& instrument, const Pcg32& noise);

    /// Starts the voice's release, unless it is already releasing or free.
    ///
    /// \param instrument The instrument, as it is at the release's frame.
    void release(const Instrument& instrument);

    /// Adds the voice's next frames into `out`, as interleaved stereo frames, and moves on by as
    /// many. A free voice adds nothing.
    ///
    /// \param out The frames to add to, two samples for each, left then right.
    /// \param scratch Room for the voice's working, at least `scratch_per_frame` floats for each
    ///                frame of `out`, whose contents are overwritten.
    /// \param block The instrument's values for these frames, at least as many.
    void render(std::span<float> out, std::span<float> scratch,
                const InstrumentBlock& block) IRAMA_NONBLOCKING;

    /// \return Whether the voice is sounding, from its start until its release ends.
    [[nodiscard]] bool active() const;

    /// \return Whether the voice is sounding and hasn't been released.
    [[nodiscard]] bool held() const;

    /// \return The note the voice last started playing.
    [[nodiscard]] int note() const;

  private:
    // Plays the next frames of the source, at the oscillator's and the noise's levels, into
    // `wave`, using `frequencies` and `other` for its working.
    void render_source(std::span<float> wave, std::span<float> frequencies, std::span<float> other,
                       const InstrumentBlock& block) IRAMA_NONBLOCKING;

    Oscillator oscillator_;
    Waveform waveform_ = Waveform::saw;
    // The oscillator a crossfade fades out, a copy of the voice's own at the change, which plays
    // the old waveform from the same phase.
    Oscillator fading_;
    Waveform fading_waveform_ = Waveform::saw;
    // How long a crossfade lasts in frames, and how many frames of the current one have been
    // played, which is as long when there is none.
    std::uint32_t crossfade_frames_ = 1;
    std::uint32_t crossfade_position_ = 1;
    Noise noise_;
    Adsr envelope_;
    // The note's frequency in hertz, before the tuning.
    float frequency_ = 0.0F;
    // The gain of the note's velocity.
    float velocity_gain_ = 0.0F;
    int note_ = 0;
};

} // namespace irama
