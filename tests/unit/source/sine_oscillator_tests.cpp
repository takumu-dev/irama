// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include <irama/engine/source/sine_oscillator.hpp>

#include <irama/engine/base/nonblocking.hpp>
#include <irama/engine/base/render_scope.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <span>

namespace {

// Renders as a voice does, inside a render scope, so the real-time checks watch the oscillator.
void render(irama::SineOscillator& oscillator, std::span<float> out) IRAMA_NONBLOCKING {
    const irama::RenderScope scope;
    oscillator.render(out);
}

} // namespace

TEST_CASE("a sine oscillator starts at phase 0 and follows a sine at its frequency", "[source]") {
    irama::SineOscillator oscillator;
    oscillator.start(1'000.0F, 48'000.0F);
    std::array<float, 480> samples{};
    render(oscillator, samples);
    for (std::size_t index = 0; index < samples.size(); ++index) {
        const double expected =
            std::sin(2.0 * std::numbers::pi * 1'000.0 * static_cast<double>(index) / 48'000.0);
        REQUIRE_THAT(samples.at(index), Catch::Matchers::WithinAbs(expected, 1e-4));
    }
}

TEST_CASE("a sine oscillator carries on from where its last render ended", "[source]") {
    irama::SineOscillator whole;
    irama::SineOscillator split;
    whole.start(440.0F, 44'100.0F);
    split.start(440.0F, 44'100.0F);
    std::array<float, 1'000> once{};
    std::array<float, 1'000> twice{};
    render(whole, once);
    render(split, std::span{twice}.first(333));
    render(split, std::span{twice}.subspan(333));
    REQUIRE(once == twice);
}

TEST_CASE("starting a sine oscillator again starts it from phase 0", "[source]") {
    irama::SineOscillator oscillator;
    oscillator.start(440.0F, 48'000.0F);
    std::array<float, 100> samples{};
    render(oscillator, samples);
    oscillator.start(880.0F, 48'000.0F);
    render(oscillator, std::span{samples}.first(1));
    REQUIRE(samples.front() == 0.0F);
}
