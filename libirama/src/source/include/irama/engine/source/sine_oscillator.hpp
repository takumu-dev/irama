// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

/// \file
/// The sine oscillator.

#pragma once

#include <irama/engine/base/nonblocking.hpp>

#include <span>

namespace irama {

/// A sine wave at a fixed frequency, at full scale.
///
/// Its phase is a `float` that counts cycles in [0, 1), and each sample is `std::sin` of
/// `2π · phase`, computed in `float`. Keeping the phase within one cycle keeps its precision the
/// same however long the oscillator runs.
class SineOscillator {
  public:
    /// Starts the wave again from phase 0, at a new frequency.
    ///
    /// \param frequency The frequency in hertz, above 0 and below half the sample rate, which is
    ///                  checked.
    /// \param sample_rate The sample rate in hertz, above 0.
    void start(float frequency, float sample_rate);

    /// Writes the next samples of the wave, one for each element of `out`, and moves on by as many.
    ///
    /// \param out Where to write the samples, overwriting what it holds.
    void render(std::span<float> out) IRAMA_NONBLOCKING;

  private:
    float phase_ = 0.0F;
    // How far the phase moves for each sample, in cycles.
    float increment_ = 0.0F;
};

} // namespace irama
