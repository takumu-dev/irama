// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

/// \file
/// White noise.

#pragma once

#include <irama/engine/base/nonblocking.hpp>
#include <irama/engine/base/pcg32.hpp>

#include <span>

namespace irama {

/// White noise, uniform from −1 up to just below +1, drawn from a `Pcg32`.
///
/// Each sample takes the top 24 bits of the generator's next output, `u`, and is `u / 2^23 − 1`,
/// which a `float` holds exactly. So the noise depends only on the generator it is given, and two
/// voices given different streams play uncorrelated noise.
class Noise {
  public:
    /// Starts the noise again from a generator.
    ///
    /// \param generator The generator to draw from, which the noise keeps its own copy of.
    void start(const Pcg32& generator);

    /// Writes the next samples of the noise, one for each element of `out`, and moves on by as
    /// many.
    ///
    /// \param out Where to write the samples, overwriting what it holds.
    void render(std::span<float> out) IRAMA_NONBLOCKING;

  private:
    Pcg32 generator_;
};

} // namespace irama
