// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include <irama/engine/source/sine_oscillator.hpp>

#include <irama/engine/base/assertion.hpp>
#include <irama/engine/base/nonblocking.hpp>

#include <cmath>
#include <numbers>
#include <span>

namespace irama {

void SineOscillator::start(float frequency, float sample_rate) {
    IRAMA_ASSERT(sample_rate > 0.0F, "a sample rate is above 0 Hz");
    IRAMA_ASSERT(frequency > 0.0F && frequency < sample_rate / 2.0F,
                 "a sine's frequency is above 0 Hz and below half the sample rate");
    phase_ = 0.0F;
    increment_ = frequency / sample_rate;
}

void SineOscillator::render(std::span<float> out) IRAMA_NONBLOCKING {
    constexpr float turn = 2.0F * std::numbers::pi_v<float>;
    for (float& sample : out) {
        sample = std::sin(turn * phase_);
        phase_ += increment_;
        if (phase_ >= 1.0F) {
            phase_ -= 1.0F;
        }
    }
}

} // namespace irama
