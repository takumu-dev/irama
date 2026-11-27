// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include <irama/engine/filter/svf.hpp>

#include <irama/engine/base/assertion.hpp>
#include <irama/engine/base/nonblocking.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <ranges>
#include <span>

namespace {

using irama::FilterMode;

// Below this magnitude a state is set to 0 at the end of a block.
constexpr float flush_below = 1e-20F;

// The resonant stage's Q is `min_q · max_q_ratio^r` for the resonance r.
constexpr float min_q = 0.5F;
constexpr float max_q_ratio = 50.0F;

[[nodiscard]] irama::SvfStage stage(float damping, float g) {
    return irama::SvfStage{
        .damping = damping,
        .feedback = damping + g,
        .scale = 1.0F / (1.0F + (damping * g) + (g * g)),
    };
}

// Filters one sample through one stage, whose two integrators' states are `state.s1` and
// `state.s2`, and returns the mode's output.
template <typename State>
[[nodiscard]] float tick(State& state, const irama::SvfStage& stage, float g, float input,
                         FilterMode mode) {
    const float high = (input - (stage.feedback * state.s1) - state.s2) * stage.scale;
    const float v1 = g * high;
    const float band = v1 + state.s1;
    state.s1 = band + v1;
    const float v2 = g * band;
    const float low = v2 + state.s2;
    state.s2 = low + v2;
    switch (mode) {
    case FilterMode::low_pass:
        return low;
    case FilterMode::high_pass:
        return high;
    case FilterMode::band_pass:
        return stage.damping * band;
    case FilterMode::notch:
        return low + high;
    }
    return low;
}

void flush(float& state) {
    if (std::abs(state) < flush_below) {
        state = 0.0F;
    }
}

} // namespace

namespace irama {

SvfCoefficients svf_coefficients(float cutoff, float resonance, float sample_rate) {
    IRAMA_ASSERT(cutoff > 0.0F, "a cutoff is above 0 Hz");
    IRAMA_ASSERT(resonance >= 0.0F && resonance <= 1.0F, "a resonance is from 0 to 1");
    const float ratio = std::min(cutoff / sample_rate, max_cutoff_ratio);
    const float g = std::tan(std::numbers::pi_v<float> * ratio);
    const float q = min_q * std::pow(max_q_ratio, resonance);
    return SvfCoefficients{
        .g = g,
        .butterworth = stage(std::numbers::sqrt2_v<float>, g),
        .resonant = stage(1.0F / q, g),
    };
}

void Svf::reset() {
    butterworth_ = {};
    resonant_ = {};
}

void Svf::render(std::span<float> samples, FilterMode mode, FilterSlope slope,
                 std::span<const SvfCoefficients> coefficients) IRAMA_NONBLOCKING {
    IRAMA_ASSERT(coefficients.size() >= samples.size(),
                 "a filter has its coefficients for each sample");
    if (slope == FilterSlope::db24) {
        for (const auto [sample, values] : std::views::zip(samples, coefficients)) {
            const float first = tick(butterworth_, values.butterworth, values.g, sample, mode);
            sample = tick(resonant_, values.resonant, values.g, first, mode);
        }
    } else {
        // The first stage isn't used, so that a change to 24 dB starts it from silence.
        butterworth_ = {};
        for (const auto [sample, values] : std::views::zip(samples, coefficients)) {
            sample = tick(resonant_, values.resonant, values.g, sample, mode);
        }
    }
    flush(butterworth_.s1);
    flush(butterworth_.s2);
    flush(resonant_.s1);
    flush(resonant_.s2);
}

} // namespace irama
