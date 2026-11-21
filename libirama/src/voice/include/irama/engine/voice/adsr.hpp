// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

/// \file
/// The attack, decay, sustain and release envelope.

#pragma once

#include <irama/engine/base/nonblocking.hpp>

#include <cstdint>
#include <span>

namespace irama {

/// The times and curves of an `Adsr`'s segments.
///
/// Each curve control runs from −1 to +1 and bends its segment: 0 is a straight line, a positive
/// curve starts fast and slows towards its end, like an analogue decay, and a negative one starts
/// slowly. The defaults are the amplifier envelope's.
struct AdsrSettings {
    /// How long the attack takes to rise from 0 to full level, in seconds, above 0.
    float attack_seconds = 0.002F;
    /// How long the decay takes to fall from full level to the sustain level, in seconds, above 0.
    float decay_seconds = 0.3F;
    /// How long the release takes to fall to 0, in seconds, above 0.
    float release_seconds = 0.2F;
    /// The attack's curve, from −1 to +1.
    float attack_curve = 0.0F;
    /// The decay's curve, from −1 to +1.
    float decay_curve = 0.5F;
    /// The release's curve, from −1 to +1.
    float release_curve = 0.5F;
};

/// An envelope of an attack, a decay, a sustain and a release, which shapes a voice's level.
///
/// Each segment lasts exactly its time, rounded to the nearest whole number of frames and at least
/// one, so a test can say on which frame a segment ends, and so can a voice. Over a segment of `n`
/// frames, its `i`th frame, counting from 0, is at progress `p = (i + 1) / n`, and its level moves
/// from the segment's start level to its end level as `start + (end − start) · shape(p)`, where
/// `shape(p) = (1 − e^(−k·p)) / (1 − e^(−k))` with `k = 6 · curve`, or `shape(p) = p` for a curve
/// of 0. So a segment's last frame is at its end level exactly.
///
/// The attack rises from 0 to 1, the decay falls from 1 towards the sustain level, and the sustain
/// holds until `release()`, which falls from wherever the level is, mid-attack included, to 0. The
/// envelope ends on the last frame of its release, with no threshold.
///
/// Each segment's time and curve are latched when it starts, from the settings given then. The
/// sustain level is live: it is given for each frame, the decay heads for it frame by frame, and
/// the sustain follows it.
class Adsr {
  public:
    /// Starts the attack from level 0.
    ///
    /// \param settings The times and curves as they are now, each within its range, which is
    ///                 checked. The attack latches its own.
    /// \param sample_rate The sample rate in hertz, above 0.
    void start(const AdsrSettings& settings, float sample_rate);

    /// Starts the release from the current level, unless the envelope is already releasing or has
    /// ended.
    ///
    /// \param settings The times and curves as they are now. The release latches its own.
    void release(const AdsrSettings& settings);

    /// Writes the envelope's next levels, one for each element of `levels`, and moves on by as
    /// many. Once the envelope has ended, every level is 0.
    ///
    /// \param levels Where to write the levels, overwriting what it holds.
    /// \param sustain The sustain level of each of those frames, from 0 to 1, at least as many.
    /// \param settings The times and curves as they are now, which a decay starting during these
    ///                 frames latches.
    void render(std::span<float> levels, std::span<const float> sustain,
                const AdsrSettings& settings) IRAMA_NONBLOCKING;

    /// \return Whether the envelope has started and not yet ended.
    [[nodiscard]] bool active() const;

    /// \return Whether the envelope is in its release.
    [[nodiscard]] bool releasing() const;

  private:
    enum class Stage : std::uint8_t { ended, attack, decay, sustain, release };

    // Starts a segment of `stage`, lasting `seconds`, from the current level to `end`, which the
    // decay ignores for the live sustain level.
    void begin(Stage stage, float seconds, float curve, float end);

    float sample_rate_ = 0.0F;
    Stage stage_ = Stage::ended;
    // The level of the last frame written.
    float level_ = 0.0F;
    // The current segment, if the stage has one: its start and end levels, its curve's `k`, its
    // length in frames, and how many of them have been written. The decay's end level is the
    // sustain level of each frame instead.
    float start_level_ = 0.0F;
    float end_level_ = 0.0F;
    float k_ = 0.0F;
    std::uint32_t length_ = 0;
    std::uint32_t position_ = 0;
};

} // namespace irama
