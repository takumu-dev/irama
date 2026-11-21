// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

/// \file
/// One voice: a note sounding from its note-on to the end of its release.

#pragma once

#include <irama/engine/base/nonblocking.hpp>
#include <irama/engine/source/sine_oscillator.hpp>
#include <irama/engine/voice/adsr.hpp>
#include <irama/engine/voice/instrument.hpp>

#include <span>

namespace irama {

/// One note, sounding from its note-on until its release ends.
///
/// A voice is a sine at the note's frequency, shaped by an `Adsr`, through the amplifier: the
/// instrument's gain and pan on each channel, and the gain of the note's velocity. It reads the
/// instrument's values as they are when it needs them: the velocity's gain and the attack's
/// settings when it starts, each later segment's settings when that segment starts, and the sustain
/// level and the amplifier's gains every frame. A voice is free until it starts, and free again on
/// the frame after its release ends.
class Voice {
  public:
    /// Starts the voice playing a note, from the start of its attack.
    ///
    /// \param note The note, from `lowest_note` to `highest_note`, which is checked.
    /// \param velocity How hard the note is played, from 0 to 1.
    /// \param instrument The instrument, as it is at the note's frame.
    void start(int note, float velocity, const Instrument& instrument);

    /// Starts the voice's release, unless it is already releasing or free.
    ///
    /// \param instrument The instrument, as it is at the release's frame.
    void release(const Instrument& instrument);

    /// Adds the voice's next frames into `out`, as interleaved stereo frames, and moves on by as
    /// many. A free voice adds nothing.
    ///
    /// \param out The frames to add to, two samples for each, left then right.
    /// \param scratch Room for the voice's working, at least as long as `out`, whose contents are
    ///                overwritten.
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
    SineOscillator oscillator_;
    Adsr envelope_;
    // The gain of the note's velocity.
    float velocity_gain_ = 0.0F;
    int note_ = 0;
};

} // namespace irama
