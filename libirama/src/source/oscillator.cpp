// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include <irama/engine/source/oscillator.hpp>

#include <irama/engine/base/assertion.hpp>
#include <irama/engine/base/nonblocking.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <ranges>
#include <span>

namespace {

using irama::Waveform;

// Wraps a phase that has gone up to one cycle past [0, 1) back into it.
[[nodiscard]] float wrap(float phase) { return phase >= 1.0F ? phase - 1.0F : phase; }

// The 2-point PolyBLEP residual of a jump of −2 at phase 0, for a sample at phase `t` from it,
// with the phase moving `dt` a sample: twice the residual of a band-limited unit step.
[[nodiscard]] float polyblep(float t, float dt) {
    if (t < dt) {
        const float x = t / dt;
        return -(1.0F - x) * (1.0F - x);
    }
    if (t > 1.0F - dt) {
        const float x = (t - 1.0F) / dt;
        return (1.0F + x) * (1.0F + x);
    }
    return 0.0F;
}

// The 2-point polyBLAMP residual of a corner at phase 0, for a sample at phase `t` from it, as
// `(1 − |x|)³`, which a corner scales by its change of slope a sample over 6.
[[nodiscard]] float polyblamp(float t, float dt) {
    if (t < dt) {
        const float x = 1.0F - (t / dt);
        return x * x * x;
    }
    if (t > 1.0F - dt) {
        const float x = 1.0F + ((t - 1.0F) / dt);
        return x * x * x;
    }
    return 0.0F;
}

// The triangle's corners turn its slope by ±8 a cycle, so by ±8·dt a sample, and each corner's
// residual is scaled by that over 6.
constexpr float corner_scale = 8.0F / 6.0F;

[[nodiscard]] float sample(Waveform waveform, float t, float dt, float width) {
    switch (waveform) {
    case Waveform::sine:
        return std::sin(2.0F * std::numbers::pi_v<float> * t);
    case Waveform::triangle: {
        // The bottom corner is at t = 0.75 and the top one at t = 0.25.
        const float from_bottom = wrap(t + 0.25F);
        const float from_top = wrap(t + 0.75F);
        const float naive = 1.0F - (4.0F * std::abs(from_bottom - 0.5F));
        return naive + (corner_scale * dt * (polyblamp(from_bottom, dt) - polyblamp(from_top, dt)));
    }
    case Waveform::saw: {
        // The jump is at t = 0.5.
        const float from_jump = wrap(t + 0.5F);
        return (2.0F * from_jump) - 1.0F - polyblep(from_jump, dt);
    }
    case Waveform::pulse: {
        // It rises at t = 0 and falls at t = width.
        const float naive = t < width ? 1.0F : -1.0F;
        return naive + polyblep(t, dt) - polyblep(wrap(t - width + 1.0F), dt);
    }
    }
    return 0.0F;
}

} // namespace

namespace irama {

void Oscillator::start(float sample_rate) {
    IRAMA_ASSERT(sample_rate > 0.0F, "a sample rate is above 0 Hz");
    sample_rate_ = sample_rate;
    phase_ = 0.0F;
    period_starts_ = true;
}

void Oscillator::render(std::span<float> out, Waveform waveform, std::span<const float> frequencies,
                        std::span<const float> widths) IRAMA_NONBLOCKING {
    IRAMA_ASSERT(frequencies.size() >= out.size() && widths.size() >= out.size(),
                 "an oscillator has a frequency and a width for each sample");
    for (const auto [value, frequency, width] : std::views::zip(out, frequencies, widths)) {
        if (period_starts_) {
            width_ = width;
            period_starts_ = false;
        }
        const float increment = std::min(frequency / sample_rate_, max_frequency_ratio);
        value = sample(waveform, phase_, increment, width_);
        phase_ += increment;
        if (phase_ >= 1.0F) {
            phase_ -= 1.0F;
            period_starts_ = true;
        }
    }
}

} // namespace irama
