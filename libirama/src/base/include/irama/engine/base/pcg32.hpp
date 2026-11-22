// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

/// \file
/// The PCG32 random number generator.

#pragma once

#include <cstdint>

namespace irama {

/// A random number generator: PCG32, the XSH-RR member of the PCG family, written from Melissa
/// O'Neill's paper "PCG: A Family of Simple Fast Space-Efficient Statistically Good Algorithms for
/// Random Number Generation" (2014).
///
/// Its state is a 64-bit linear congruential generator, which moves on by `state · a + c` for each
/// output, modulo 2^64. Each output is 32 bits of the state before the step: its top bits xored
/// with themselves shifted right by 18, keeping bits 27 to 58, rotated right by the state's top
/// five bits. The **seed** chooses the starting point, and the **stream** chooses `c`, which is
/// always odd, so each of the 2^63 streams has a period of 2^64.
///
/// A generator is a plain value: it allocates nothing, and copying it copies its sequence. The
/// same seed and stream always give the same outputs, on every target.
class Pcg32 {
  public:
    /// Makes the generator of seed 0 on stream 0.
    constexpr Pcg32() : Pcg32{0, 0} {}

    /// Makes a generator, as the paper seeds one: the state starts at 0, steps once, adds the
    /// seed, and steps again.
    ///
    /// \param seed Where the sequence starts.
    /// \param stream Which of the streams to follow. Its top bit is ignored.
    constexpr Pcg32(std::uint64_t seed, std::uint64_t stream) : increment_{(stream << 1U) | 1U} {
        step();
        state_ += seed;
        step();
    }

    /// \return The next output, any 32-bit value with equal chance.
    [[nodiscard]] constexpr std::uint32_t next() {
        const std::uint64_t old = state_;
        step();
        const auto shifted = static_cast<std::uint32_t>(((old >> 18U) ^ old) >> 27U);
        const auto rotation = static_cast<std::uint32_t>(old >> 59U);
        return (shifted >> rotation) | (shifted << ((32U - rotation) & 31U));
    }

    /// Draws a new generator from this one, so that each user of randomness can have a sequence of
    /// its own that depends only on this generator.
    ///
    /// \return The generator whose seed is the next two outputs and whose stream is the two after,
    ///         each pair joined with the first output as the high 32 bits.
    [[nodiscard]] constexpr Pcg32 split() {
        const std::uint64_t seed = (std::uint64_t{next()} << 32U) | next();
        const std::uint64_t stream = (std::uint64_t{next()} << 32U) | next();
        return Pcg32{seed, stream};
    }

  private:
    // The paper's multiplier for 64-bit state.
    static constexpr std::uint64_t multiplier = 6'364'136'223'846'793'005U;

    constexpr void step() { state_ = (state_ * multiplier) + increment_; }

    std::uint64_t state_ = 0;
    std::uint64_t increment_;
};

} // namespace irama
