// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

/// \file
/// One voice: a note sounding from its note-on to the end of its release.

#pragma once

#include <irama/engine/base/nonblocking.hpp>
#include <irama/engine/base/pcg32.hpp>
#include <irama/engine/filter/svf.hpp>
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
/// fine offsets, at the oscillator's level, plus `Noise` at the noise's level, through an `Svf` in
/// the instrument's filter mode and slope, shaped by an `Adsr`, through the amplifier: the
/// instrument's gain and pan on each channel, or its gain and the voice's own fixed pan, and the
/// gain of the note's velocity. It reads the
/// instrument's values as they are when it needs them: the velocity's gain, the waveform, the
/// filter's mode and slope and the attack's settings when it starts, each later segment's settings
/// when that segment starts, and the source's values, the filter's coefficients, the sustain level
/// and the amplifier's gains every frame. A voice is free until it starts, and free again on the
/// frame after its release ends. It starts its filter from silence.
///
/// A change of waveform, filter mode or filter slope while the voice sounds crossfades linearly
/// over 5 ms, rounded to the nearest whole number of frames: the voice plays its old configuration
/// on a copy of its oscillator and filter, beside the new one on its own, from the same phase and
/// the same filter state, and frame `i`, counting from 0, of `n` is `(i + 1) / n` of the new one,
/// so the last frame is the new configuration alone. A change during a crossfade starts a new one,
/// from the configuration that was fading in. A coarse step is a jump in pitch with a continuous
/// phase, and doesn't crossfade.
///
/// A stolen voice fades out linearly over 5 ms, rounded to the nearest whole number of frames, from
/// wherever its envelope is: frame `i`, counting from 0, of `n` is at `1 − (i + 1) / n` of the
/// voice's level, so the last frame is silent, and the voice is free on the frame after it. A
/// player fades it in a copy, in one of its fade slots, so the voice itself can start the new note.
class Voice {
  public:
    /// How many floats of scratch `render()` needs for each frame.
    static constexpr std::size_t scratch_per_frame = 3;

    /// How long a crossfade between configurations lasts, in seconds.
    static constexpr float crossfade_seconds = 0.005F;

    /// How long a stolen voice's fade lasts, in seconds.
    static constexpr float steal_fade_seconds = 0.005F;

    /// Starts the voice playing a note, from the start of its attack.
    ///
    /// \param note The note, from `lowest_note` to `highest_note`, which is checked.
    /// \param velocity How hard the note is played, from 0 to 1.
    /// \param instrument The instrument, as it is at the note's frame.
    /// \param noise The generator the voice's noise draws from, which it keeps its own copy of.
    /// \param order The note's place among the notes its player has started, larger for a later
    ///              one, by which stealing finds the oldest voice. A voice outside a player can
    ///              leave it at 0.
    void start(int note, float velocity, const Instrument& instrument, const Pcg32& noise,
               std::uint64_t order = 0);

    /// Fixes the voice's pan, in place of the instrument's, until its next start. A fixed pan isn't
    /// smoothed.
    ///
    /// \param position The pan, from −1, hard left, to +1, hard right.
    void fix_pan(float position);

    /// Starts the voice's release, unless it is already releasing or free.
    ///
    /// \param instrument The instrument, as it is at the release's frame.
    void release(const Instrument& instrument);

    /// Starts the voice's fade, which ends it 5 ms later, as a stolen voice does. The voice is
    /// sounding and not already fading, which is checked where `NDEBUG` isn't defined.
    void fade_out();

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

    /// \return Whether the voice is sounding and hasn't been released or begun to fade.
    [[nodiscard]] bool held() const;

    /// \return Whether the voice is sounding and in its release.
    [[nodiscard]] bool releasing() const;

    /// \return Whether the voice is sounding and fading out.
    [[nodiscard]] bool fading() const;

    /// \return How many frames of its fade a fading voice has left, the last, silent one included.
    [[nodiscard]] std::uint32_t fade_frames_left() const;

    /// \return The voice's level at its latest frame, before the amplifier's gain and pan: its
    ///         envelope's level times its velocity's gain, which is 0 before its first frame.
    [[nodiscard]] float level() const;

    /// \return The order the voice was last started with.
    [[nodiscard]] std::uint64_t order() const;

    /// \return The note the voice last started playing.
    [[nodiscard]] int note() const;

  private:
    // One configuration of the voice's oscillator and filter, with the state each has reached.
    struct Chain {
        Oscillator oscillator;
        Waveform waveform = Waveform::saw;
        Svf filter;
        FilterMode filter_mode = FilterMode::low_pass;
        FilterSlope filter_slope = FilterSlope::db12;
    };

    // Plays the next frames of the source through the filter into `wave`, crossfading from the
    // old configuration if one is fading out, using `other` and `third` for its working.
    void render_chains(std::span<float> wave, std::span<float> other, std::span<float> third,
                       const InstrumentBlock& block) IRAMA_NONBLOCKING;

    Chain chain_;
    // The configuration a crossfade fades out, a copy of the voice's own at the change.
    Chain fading_;
    // How long a crossfade lasts in frames, and how many frames of the current one have been
    // played, which is as long when there is none.
    std::uint32_t crossfade_frames_ = 1;
    std::uint32_t crossfade_position_ = 1;
    // How long a steal's fade lasts in frames, whether the voice has been stolen and is fading,
    // and how many frames of the fade have been played.
    std::uint32_t fade_frames_ = 1;
    bool stolen_ = false;
    std::uint32_t fade_position_ = 0;
    Noise noise_;
    Adsr envelope_;
    // The note's frequency in hertz, before the tuning.
    float frequency_ = 0.0F;
    // The gain of the note's velocity.
    float velocity_gain_ = 0.0F;
    // Whether the voice has a pan of its own, and its gain on each channel.
    bool pan_fixed_ = false;
    float left_pan_ = 0.0F;
    float right_pan_ = 0.0F;
    std::uint64_t order_ = 0;
    int note_ = 0;
};

} // namespace irama
