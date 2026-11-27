// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include <irama/engine/filter/svf.hpp>

#include <irama/engine/base/nonblocking.hpp>
#include <irama/engine/base/render_scope.hpp>
#include <irama/test_support/spectrum.hpp>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <numbers>
#include <span>
#include <vector>

// The flush is tested with flush-to-zero and denormals-are-zero off, which only x86-64 has.
#if defined(__x86_64__) || defined(_M_X64)
#include <xmmintrin.h>
#endif

namespace {

using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;
using irama::FilterMode;
using irama::FilterSlope;

constexpr float rate = 48'000.0F;
// The same rate, for the analytic response, which is computed in double.
constexpr double exact_rate = 48'000.0;

// Renders as a voice does, inside a render scope, so the real-time checks watch the filter.
void render(irama::Svf& filter, std::span<float> samples, FilterMode mode, FilterSlope slope,
            std::span<const irama::SvfCoefficients> coefficients) IRAMA_NONBLOCKING {
    const irama::RenderScope scope;
    filter.render(samples, mode, slope, coefficients);
}

// Filters samples through a new filter at one cutoff and one resonance.
[[nodiscard]] std::vector<float> filtered(std::vector<float> samples, FilterMode mode,
                                          FilterSlope slope, float cutoff, float resonance) {
    irama::Svf filter;
    const std::vector<irama::SvfCoefficients> coefficients(
        samples.size(), irama::svf_coefficients(cutoff, resonance, rate));
    render(filter, samples, mode, slope, coefficients);
    return samples;
}

// The response of one analogue two-pole stage of a damping, normalised to a cutoff of 1 rad/s.
[[nodiscard]] std::complex<double> analogue_stage(FilterMode mode, double damping,
                                                  std::complex<double> s) {
    const std::complex<double> denominator = (s * s) + (damping * s) + 1.0;
    switch (mode) {
    case FilterMode::low_pass:
        return 1.0 / denominator;
    case FilterMode::high_pass:
        return s * s / denominator;
    case FilterMode::band_pass:
        return damping * s / denominator;
    case FilterMode::notch:
        return ((s * s) + 1.0) / denominator;
    }
    return 0.0;
}

// The filter's response at a frequency: the analogue one's under the bilinear transform prewarped
// at the cutoff, so that s = j · tan(π·f/fs) / tan(π·fc/fs).
[[nodiscard]] std::complex<double> analytic_response(FilterMode mode, FilterSlope slope,
                                                     double cutoff, double resonance,
                                                     double frequency) {
    const double held = std::min(cutoff, 0.45 * exact_rate);
    const std::complex<double> s{0.0, std::tan(std::numbers::pi * frequency / exact_rate) /
                                          std::tan(std::numbers::pi * held / exact_rate)};
    const double q = 0.5 * std::pow(50.0, resonance);
    std::complex<double> response = analogue_stage(mode, 1.0 / q, s);
    if (slope == FilterSlope::db24) {
        response *= analogue_stage(mode, std::numbers::sqrt2, s);
    }
    return response;
}

[[nodiscard]] double decibels(double magnitude) { return 20.0 * std::log10(magnitude); }

} // namespace

TEST_CASE("the filter's response is the prewarped bilinear one, within 0.1 dB below 0.4 fs",
          "[filter]") {
    const auto mode = GENERATE(FilterMode::low_pass, FilterMode::high_pass, FilterMode::band_pass,
                               FilterMode::notch);
    const auto slope = GENERATE(FilterSlope::db12, FilterSlope::db24);
    const float cutoff = GENERATE(100.0F, 1'000.0F, 5'000.0F, 15'000.0F);
    const float resonance = GENERATE(0.0F, 1.0F);
    CAPTURE(mode, slope, cutoff, resonance);

    // The impulse response, long enough to have rung out at the highest Q, is the response's
    // inverse transform.
    constexpr std::size_t size = 65'536;
    std::vector<float> impulse(size, 0.0F);
    impulse.front() = 1.0F;
    const auto response = filtered(impulse, mode, slope, cutoff, resonance);
    std::vector<std::complex<double>> spectrum(response.begin(), response.end());
    irama_test_support::fft(spectrum);

    // Below 60 dB down, float's rounding outweighs the response itself.
    constexpr double floor = -60.0;
    std::size_t compared = 0;
    for (std::size_t bin = 0; static_cast<double>(bin) < 0.4 * size; ++bin) {
        const double frequency = static_cast<double>(bin) * exact_rate / static_cast<double>(size);
        const double expected = decibels(std::abs(analytic_response(
            mode, slope, static_cast<double>(cutoff), static_cast<double>(resonance), frequency)));
        if (expected < floor) {
            continue;
        }
        CAPTURE(frequency);
        REQUIRE_THAT(decibels(std::abs(spectrum.at(bin))), WithinAbs(expected, 0.1));
        ++compared;
    }
    // At least the passband, or the band around the cutoff, is compared.
    REQUIRE(compared > 500);
}

TEST_CASE("the resonance gives the resonant stage Q = 0.5 * 50^r, and the other is Butterworth",
          "[filter]") {
    const float resonance = GENERATE(0.0F, 0.25F, 0.5F, 1.0F);
    const irama::SvfCoefficients coefficients = irama::svf_coefficients(1'000.0F, resonance, rate);
    const double q = 0.5 * std::pow(50.0, static_cast<double>(resonance));
    REQUIRE_THAT(coefficients.resonant.damping, WithinRel(1.0 / q, 1e-5));
    REQUIRE_THAT(coefficients.butterworth.damping, WithinRel(std::numbers::sqrt2, 1e-6));
    REQUIRE_THAT(coefficients.g,
                 WithinRel(std::tan(std::numbers::pi * 1'000.0 / exact_rate), 1e-6));
}

TEST_CASE("the cutoff is held at 0.45 of the sample rate", "[filter]") {
    const irama::SvfCoefficients held = irama::svf_coefficients(23'000.0F, 0.5F, rate);
    const irama::SvfCoefficients highest = irama::svf_coefficients(0.45F * rate, 0.5F, rate);
    REQUIRE(held.g == highest.g);
    REQUIRE(held.resonant.scale == highest.resonant.scale);
    REQUIRE(irama::svf_coefficients(21'000.0F, 0.5F, rate).g < highest.g);
}

TEST_CASE("a filter starts again from silence once reset", "[filter]") {
    const auto mode = GENERATE(FilterMode::low_pass, FilterMode::high_pass);
    const auto slope = GENERATE(FilterSlope::db12, FilterSlope::db24);
    std::vector<float> noise(1'000);
    for (std::size_t index = 0; index < noise.size(); ++index) {
        noise.at(index) = std::sin(static_cast<float>(index) * 0.37F);
    }
    const std::vector<irama::SvfCoefficients> coefficients(
        noise.size(), irama::svf_coefficients(800.0F, 0.7F, rate));

    irama::Svf filter;
    render(filter, noise, mode, slope, coefficients);
    filter.reset();
    std::vector<float> again(noise.size(), 0.0F);
    again.front() = 1.0F;
    std::vector<float> fresh = again;
    render(filter, again, mode, slope, coefficients);
    irama::Svf new_filter;
    render(new_filter, fresh, mode, slope, coefficients);
    REQUIRE(again == fresh);
}

TEST_CASE("a filter's 24 dB slope starts its first stage from silence after the 12 dB slope",
          "[filter]") {
    std::vector<float> signal(500);
    for (std::size_t index = 0; index < signal.size(); ++index) {
        signal.at(index) = std::sin(static_cast<float>(index) * 0.11F);
    }
    const std::vector<irama::SvfCoefficients> coefficients(
        signal.size(), irama::svf_coefficients(2'000.0F, 0.0F, rate));

    // The filter runs the 24 dB slope, then the 12 dB, which leaves its first stage silent, and
    // then silence, until its resonant stage has rung out to exact zeros.
    irama::Svf filter;
    auto first = signal;
    render(filter, first, FilterMode::low_pass, FilterSlope::db24, coefficients);
    auto second = signal;
    render(filter, second, FilterMode::low_pass, FilterSlope::db12, coefficients);
    std::vector<float> silence;
    for (int block = 0; block < 100; ++block) {
        silence.assign(signal.size(), 0.0F);
        render(filter, silence, FilterMode::low_pass, FilterSlope::db12, coefficients);
    }
    REQUIRE(std::ranges::all_of(silence, [](float sample) { return sample == 0.0F; }));
    // So it is as good as new.
    auto after = signal;
    render(filter, after, FilterMode::low_pass, FilterSlope::db24, coefficients);
    REQUIRE(after == filtered(signal, FilterMode::low_pass, FilterSlope::db24, 2'000.0F, 0.0F));
}

#if defined(__x86_64__) || defined(_M_X64)

TEST_CASE("a filter sets state below 1e-20 to zero at the end of a block, with FTZ and DAZ off",
          "[filter]") {
    // The thread's MXCSR, put back as it was however the case ends.
    struct KeptControl {
        unsigned int saved = _mm_getcsr();
        KeptControl() = default;
        KeptControl(const KeptControl&) = delete;
        KeptControl& operator=(const KeptControl&) = delete;
        KeptControl(KeptControl&&) = delete;
        KeptControl& operator=(KeptControl&&) = delete;

        ~KeptControl() { _mm_setcsr(saved); }
    };

    constexpr unsigned int flush_to_zero = 1U << 15U;
    constexpr unsigned int denormals_are_zero = 1U << 6U;
    const KeptControl kept;
    _mm_setcsr(kept.saved & ~(flush_to_zero | denormals_are_zero));

    const auto slope = GENERATE(FilterSlope::db12, FilterSlope::db24);
    // An impulse of 1e-25 leaves every state below 1e-20 by the end of its block, and one of 1e-15
    // leaves some above it.
    const float height = GENERATE(1e-25F, 1e-15F);
    CAPTURE(slope, height);
    const std::vector<irama::SvfCoefficients> coefficients(
        64, irama::svf_coefficients(1'000.0F, 0.0F, rate));
    irama::Svf filter;
    std::vector<float> block(64, 0.0F);
    block.front() = height;
    // Outside a render scope, so the thread's own floating-point environment holds.
    filter.render(block, FilterMode::low_pass, slope, coefficients);
    REQUIRE(block.back() != 0.0F);

    std::vector<float> next(64, 0.0F);
    filter.render(next, FilterMode::low_pass, slope, coefficients);
    const bool silent = std::ranges::all_of(next, [](float sample) { return sample == 0.0F; });
    REQUIRE(silent == (height < 1e-20F));
}

#endif
