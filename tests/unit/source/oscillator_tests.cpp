// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include <irama/engine/source/oscillator.hpp>

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
#include <cstddef>
#include <numbers>
#include <span>
#include <string_view>
#include <vector>

namespace {

using Catch::Matchers::WithinAbs;
using irama::Waveform;

constexpr float rate = 48'000.0F;

// Renders as a voice does, inside a render scope, so the real-time checks watch the oscillator.
void render(irama::Oscillator& oscillator, std::span<float> out, Waveform waveform,
            std::span<const float> frequencies, std::span<const float> widths) IRAMA_NONBLOCKING {
    const irama::RenderScope scope;
    oscillator.render(out, waveform, frequencies, widths);
}

// Renders a new oscillator's first frames at one frequency and one width.
[[nodiscard]] std::vector<float> render(Waveform waveform, float frequency, std::size_t frames,
                                        float width = 0.5F) {
    irama::Oscillator oscillator;
    oscillator.start(rate);
    std::vector<float> out(frames);
    const std::vector<float> frequencies(frames, frequency);
    const std::vector<float> widths(frames, width);
    render(oscillator, out, waveform, frequencies, widths);
    return out;
}

// The phase of each frame, moved on as the oscillator moves its own: by `frequency / rate` in
// float, less 1 at each wrap.
[[nodiscard]] std::vector<float> phases(float frequency, std::size_t frames) {
    const float increment = frequency / rate;
    std::vector<float> result(frames);
    float phase = 0.0F;
    for (float& value : result) {
        value = phase;
        phase += increment;
        if (phase >= 1.0F) {
            phase -= 1.0F;
        }
    }
    return result;
}

[[nodiscard]] double wrap(double phase) { return phase - std::floor(phase); }

// The naive waveforms, each starting at 0 and rising, in phase with the sine.
[[nodiscard]] double naive(Waveform waveform, double phase, double width) {
    switch (waveform) {
    case Waveform::sine:
        return std::sin(2.0 * std::numbers::pi * phase);
    case Waveform::triangle:
        return 1.0 - (4.0 * std::abs(wrap(phase + 0.25) - 0.5));
    case Waveform::saw:
        return (2.0 * wrap(phase + 0.5)) - 1.0;
    case Waveform::pulse:
        return phase < width ? 1.0 : -1.0;
    }
    return 0.0;
}

// The 2-point PolyBLEP residual of a step of −2 at phase 0, `t` from it, for an increment `dt`.
[[nodiscard]] double polyblep(double t, double dt) {
    if (t < dt) {
        const double x = t / dt;
        return -(1.0 - x) * (1.0 - x);
    }
    if (t > 1.0 - dt) {
        const double x = (t - 1.0) / dt;
        return (1.0 + x) * (1.0 + x);
    }
    return 0.0;
}

// The 2-point polyBLAMP residual of a corner at phase 0, `t` from it, as `(1 − |x|)³`.
[[nodiscard]] double polyblamp(double t, double dt) {
    if (t < dt) {
        const double x = 1.0 - (t / dt);
        return x * x * x;
    }
    if (t > 1.0 - dt) {
        const double x = 1.0 + ((t - 1.0) / dt);
        return x * x * x;
    }
    return 0.0;
}

// The band-limited waveforms, as the oscillator's documentation gives them.
[[nodiscard]] double band_limited(Waveform waveform, double phase, double dt, double width) {
    switch (waveform) {
    case Waveform::sine:
        return naive(waveform, phase, width);
    case Waveform::triangle:
        return naive(waveform, phase, width) +
               ((4.0 / 3.0) * dt *
                (polyblamp(wrap(phase + 0.25), dt) - polyblamp(wrap(phase + 0.75), dt)));
    case Waveform::saw:
        return naive(waveform, phase, width) - polyblep(wrap(phase + 0.5), dt);
    case Waveform::pulse:
        return naive(waveform, phase, width) + polyblep(phase, dt) -
               polyblep(wrap(phase - width), dt);
    }
    return 0.0;
}

// Renders the naive waveform with the oscillator's phases, as the aliasing test's reference.
[[nodiscard]] std::vector<float> render_naive(Waveform waveform, float frequency,
                                              std::size_t frames, float width) {
    const auto phase = phases(frequency, frames);
    std::vector<float> out(frames);
    for (std::size_t frame = 0; frame < frames; ++frame) {
        out.at(frame) = static_cast<float>(
            naive(waveform, static_cast<double>(phase.at(frame)), static_cast<double>(width)));
    }
    return out;
}

constexpr std::size_t analysis_size = 65'536;

// The aliasing test counts the components up to 8 kHz. Below it lie the aliases folded down from
// near the sample rate, which a 2-point correction all but removes, and which stand out from the
// harmonics. Above it lie the harmonics just past half the sample rate, folded back just below it,
// which the correction weakens by only about 8 dB: its residual's spectrum is sinc², 0.4 at half
// the sample rate.
constexpr double highest_counted = 8'000.0;

// One case of the aliasing test, and its limit: the first implementation's measurement plus 3 dB.
struct AliasingCase {
    std::string_view name;
    Waveform waveform;
    float width;
    float frequency;
    double limit;
};

} // namespace

TEST_CASE("an oscillator's sine starts at phase 0 and follows a sine at its frequency",
          "[source]") {
    const auto out = render(Waveform::sine, 1'000.0F, 480);
    for (std::size_t index = 0; index < out.size(); ++index) {
        const double expected =
            std::sin(2.0 * std::numbers::pi * 1'000.0 * static_cast<double>(index) / 48'000.0);
        REQUIRE_THAT(out.at(index), WithinAbs(expected, 1e-4));
    }
}

TEST_CASE("each waveform starts at 0 and rises, in phase with the sine", "[source]") {
    const Waveform waveform = GENERATE(Waveform::sine, Waveform::triangle, Waveform::saw);
    const auto out = render(waveform, 100.0F, 2);
    REQUIRE(out.at(0) == 0.0F);
    REQUIRE(out.at(1) > 0.0F);
}

TEST_CASE("each waveform is band-limited as its formula says", "[source]") {
    const Waveform waveform =
        GENERATE(Waveform::sine, Waveform::triangle, Waveform::saw, Waveform::pulse);
    const float width = GENERATE(0.5F, 0.1F, 0.95F);
    // Neither frequency divides the rate, so the discontinuities fall between samples.
    const float frequency = GENERATE(1'234.5F, 6'543.2F);
    constexpr std::size_t frames = 2'000;
    const auto out = render(waveform, frequency, frames, width);
    const auto phase = phases(frequency, frames);
    const auto dt = static_cast<double>(frequency / rate);
    for (std::size_t frame = 0; frame < frames; ++frame) {
        REQUIRE_THAT(out.at(frame),
                     WithinAbs(band_limited(waveform, static_cast<double>(phase.at(frame)), dt,
                                            static_cast<double>(width)),
                               1e-5));
    }
}

TEST_CASE("away from its edges and corners each waveform is the naive one", "[source]") {
    const Waveform waveform = GENERATE(Waveform::triangle, Waveform::saw, Waveform::pulse);
    constexpr float frequency = 1'234.5F;
    constexpr std::size_t frames = 2'000;
    const auto out = render(waveform, frequency, frames, 0.3F);
    const auto phase = phases(frequency, frames);
    const auto dt = static_cast<double>(frequency / rate);
    std::size_t naive_frames = 0;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const auto t = static_cast<double>(phase.at(frame));
        bool near_edge = false;
        for (const double edge : {0.0, 0.25, 0.3, 0.5, 0.75}) {
            const double distance = std::abs(t - edge);
            near_edge = near_edge || std::min(distance, 1.0 - distance) < dt;
        }
        if (!near_edge) {
            REQUIRE_THAT(out.at(frame), WithinAbs(naive(waveform, t, 0.3), 1e-6));
            ++naive_frames;
        }
    }
    REQUIRE(naive_frames > frames / 2);
}

TEST_CASE("an oscillator carries on from where its last render ended", "[source]") {
    const Waveform waveform =
        GENERATE(Waveform::sine, Waveform::triangle, Waveform::saw, Waveform::pulse);
    irama::Oscillator whole;
    irama::Oscillator split;
    whole.start(44'100.0F);
    split.start(44'100.0F);
    const std::vector<float> frequencies(1'000, 440.0F);
    const std::vector<float> widths(1'000, 0.3F);
    std::vector<float> once(1'000);
    std::vector<float> twice(1'000);
    render(whole, once, waveform, frequencies, widths);
    render(split, std::span{twice}.first(333), waveform, std::span{frequencies}.first(333),
           std::span{widths}.first(333));
    render(split, std::span{twice}.subspan(333), waveform, std::span{frequencies}.subspan(333),
           std::span{widths}.subspan(333));
    REQUIRE(once == twice);
}

TEST_CASE("starting an oscillator again starts it from phase 0", "[source]") {
    irama::Oscillator oscillator;
    oscillator.start(rate);
    std::vector<float> out(100);
    const std::vector<float> frequencies(100, 440.0F);
    const std::vector<float> widths(100, 0.5F);
    render(oscillator, out, Waveform::sine, frequencies, widths);
    oscillator.start(rate);
    render(oscillator, std::span{out}.first(1), Waveform::sine, frequencies, widths);
    REQUIRE(out.front() == 0.0F);
}

TEST_CASE("a change of frequency keeps the phase continuous", "[source]") {
    // A coarse step of an octave at frame 300.
    std::vector<float> frequencies(600, 440.0F);
    std::fill(frequencies.begin() + 300, frequencies.end(), 880.0F);
    const std::vector<float> widths(600, 0.5F);
    irama::Oscillator oscillator;
    oscillator.start(rate);
    std::vector<float> out(600);
    render(oscillator, out, Waveform::sine, frequencies, widths);
    double phase = 0.0;
    for (std::size_t frame = 0; frame < out.size(); ++frame) {
        REQUIRE_THAT(out.at(frame), WithinAbs(std::sin(2.0 * std::numbers::pi * phase), 1e-4));
        phase += static_cast<double>(frequencies.at(frame)) / 48'000.0;
    }
}

TEST_CASE("an oscillator latches the pulse width at the phase wrap", "[source]") {
    // At 480 Hz a period is 100 frames. The width changes at frame 130, inside the second period,
    // where a pulse 50 % wide is high and one 20 % wide is low.
    constexpr std::size_t frames = 400;
    const std::vector<float> frequencies(frames, 480.0F);
    std::vector<float> widths(frames, 0.5F);
    std::fill(widths.begin() + 130, widths.end(), 0.2F);
    irama::Oscillator oscillator;
    oscillator.start(rate);
    std::vector<float> out(frames);
    render(oscillator, out, Waveform::pulse, frequencies, widths);
    const auto wide = render(Waveform::pulse, 480.0F, frames, 0.5F);
    const auto narrow = render(Waveform::pulse, 480.0F, frames, 0.2F);

    // Wide until the period ends, near frame 200, and narrow from the next. Near the wrap both
    // pulses are low, then high, alike.
    REQUIRE(wide.at(140) != narrow.at(140));
    REQUIRE(wide.at(230) != narrow.at(230));
    for (std::size_t frame = 0; frame < 195; ++frame) {
        REQUIRE(out.at(frame) == wide.at(frame));
    }
    for (std::size_t frame = 195; frame < frames; ++frame) {
        REQUIRE(out.at(frame) == narrow.at(frame));
    }
}

TEST_CASE("an oscillator latches its first period's width when it starts", "[source]") {
    std::vector<float> widths(100, 0.2F);
    std::fill(widths.begin() + 10, widths.end(), 0.5F);
    const std::vector<float> frequencies(100, 480.0F);
    irama::Oscillator oscillator;
    oscillator.start(rate);
    std::vector<float> out(100);
    render(oscillator, out, Waveform::pulse, frequencies, widths);
    REQUIRE(out == render(Waveform::pulse, 480.0F, 100, 0.2F));
}

TEST_CASE("an oscillator holds a frequency above 0.45 of the rate at 0.45 of it", "[source]") {
    const Waveform waveform = GENERATE(Waveform::sine, Waveform::saw);
    REQUIRE(render(waveform, 30'000.0F, 200) == render(waveform, 0.45F * rate, 200));
    REQUIRE(render(waveform, 21'000.0F, 200) != render(waveform, 0.45F * rate, 200));
}

TEST_CASE("the band-limited waveforms alias less than their limits and the naive waveforms",
          "[source]") {
    // Each limit is the first implementation's measurement plus 3 dB, and must beat the naive
    // waveform at the same note by at least 20 dB.
    const AliasingCase test_case = GENERATE(values<AliasingCase>({
        {"saw at 110 Hz", Waveform::saw, 0.5F, 110.0F, -77.0},
        {"saw at 440 Hz", Waveform::saw, 0.5F, 440.0F, -64.9},
        {"saw at 1,760 Hz", Waveform::saw, 0.5F, 1'760.0F, -54.3},
        {"pulse 50 % at 110 Hz", Waveform::pulse, 0.5F, 110.0F, -77.5},
        {"pulse 50 % at 440 Hz", Waveform::pulse, 0.5F, 440.0F, -64.9},
        {"pulse 50 % at 1,760 Hz", Waveform::pulse, 0.5F, 1'760.0F, -54.3},
        {"pulse 10 % at 110 Hz", Waveform::pulse, 0.1F, 110.0F, -67.3},
        {"pulse 10 % at 440 Hz", Waveform::pulse, 0.1F, 440.0F, -58.9},
        {"pulse 10 % at 1,760 Hz", Waveform::pulse, 0.1F, 1'760.0F, -45.9},
        {"triangle at 110 Hz", Waveform::triangle, 0.5F, 110.0F, -128.7},
        {"triangle at 440 Hz", Waveform::triangle, 0.5F, 440.0F, -104.1},
        {"triangle at 1,760 Hz", Waveform::triangle, 0.5F, 1'760.0F, -81.5},
    }));
    CAPTURE(test_case.name);
    const auto band_limited =
        render(test_case.waveform, test_case.frequency, analysis_size, test_case.width);
    const auto naive =
        render_naive(test_case.waveform, test_case.frequency, analysis_size, test_case.width);
    const double measured = irama_test_support::strongest_non_harmonic(
        band_limited, static_cast<double>(test_case.frequency), static_cast<double>(rate),
        highest_counted);
    const double naive_measured =
        irama_test_support::strongest_non_harmonic(naive, static_cast<double>(test_case.frequency),
                                                   static_cast<double>(rate), highest_counted);
    CAPTURE(measured, naive_measured);
    REQUIRE(measured <= test_case.limit);
    REQUIRE(test_case.limit <= naive_measured - 20.0);
}
