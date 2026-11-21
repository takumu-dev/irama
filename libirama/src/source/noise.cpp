// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include <irama/engine/source/noise.hpp>

#include <irama/engine/base/nonblocking.hpp>
#include <irama/engine/base/pcg32.hpp>

#include <cstdint>
#include <span>

namespace {

// 2^23, so that the top 24 bits of an output span two units.
constexpr float half_range = 8'388'608.0F;

// How far an output is shifted right to keep its top 24 bits.
constexpr std::uint32_t dropped_bits = 8;

} // namespace

namespace irama {

void Noise::start(const Pcg32& generator) { generator_ = generator; }

void Noise::render(std::span<float> out) IRAMA_NONBLOCKING {
    for (float& sample : out) {
        sample = (static_cast<float>(generator_.next() >> dropped_bits) / half_range) - 1.0F;
    }
}

} // namespace irama
