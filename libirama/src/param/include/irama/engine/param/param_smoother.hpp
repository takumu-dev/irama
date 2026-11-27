// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

/// \file
/// The smoother that glides a parameter to each new value.

#pragma once

#include <irama/engine/base/nonblocking.hpp>
#include <irama/engine/param/param_descriptor.hpp>

#include <cstddef>
#include <cstdint>
#include <span>

namespace irama {

/// The live value of one parameter, which glides to each new value it is set to.
///
/// A smoothed set starts a **linear ramp in the normalised domain**, so a logarithmic parameter
/// such as a cutoff glides evenly in octaves. The ramp lasts exactly the descriptor's smoothing
/// time, rounded to the nearest whole number of frames and at least one. Its `i`th frame, counting
/// from 0, of `n` is at progress `(i + 1) / n`, so its last frame is at the new value exactly. A
/// new value set during a ramp starts a new ramp, of the full time, from the value of the latest
/// frame. A parameter with no smoothing time, a stepped one among them, switches at once.
///
/// The smoother is set between blocks, never during one: `render()` writes a block's values, and
/// `advance()` moves on past them.
class ParamSmoother {
  public:
    /// Makes a smoother at the parameter's default, at rest.
    ///
    /// \param descriptor The parameter, which must outlive the smoother, as a table's does.
    /// \param sample_rate The sample rate in hertz, above 0.
    ParamSmoother(const ParamDescriptor& descriptor, float sample_rate);

    /// Sets a new value, which the smoother ramps to or jumps to.
    ///
    /// \param value The new plain value, already constrained to the parameter.
    /// \param smooth Whether to ramp to it, as the descriptor's smoothing time says, rather than
    ///               jump. A player ramps only while a voice sounds.
    void set(float value, bool smooth) IRAMA_NONBLOCKING;

    /// Writes the plain values of the next frames, one for each element of `values`, without moving
    /// on.
    ///
    /// \param values Where to write them, overwriting what it holds.
    void render(std::span<float> values) const IRAMA_NONBLOCKING;

    /// Moves on by a number of frames.
    ///
    /// \param frames How many frames to move on by.
    void advance(std::size_t frames) IRAMA_NONBLOCKING;

    /// \return The plain value of the latest frame moved past, or the value jumped to since.
    [[nodiscard]] float value() const;

    /// \return The latest plain value set.
    [[nodiscard]] float target() const;

    /// \return Whether a ramp is under way, so that `value()` hasn't yet reached `target()`.
    [[nodiscard]] bool ramping() const;

  private:
    // The plain value at frame `position` of the ramp.
    [[nodiscard]] float at(std::uint32_t position) const;

    const ParamDescriptor* descriptor_;
    // How many frames a ramp takes, or 0 for none.
    std::uint32_t ramp_frames_;
    float value_;
    float target_;
    // The ramp: its start and end as normalised values, its length in frames, and how many of them
    // have been moved past. A smoother at rest has moved past all of them.
    float start_ = 0.0F;
    float end_ = 0.0F;
    std::uint32_t length_ = 0;
    std::uint32_t position_ = 0;
};

} // namespace irama
