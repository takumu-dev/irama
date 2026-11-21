// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include <irama/engine/param/param_smoother.hpp>

#include <irama/engine/base/nonblocking.hpp>
#include <irama/engine/base/render_scope.hpp>
#include <irama/engine/param/param_descriptor.hpp>
#include <irama/engine/param/param_id.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cstddef>
#include <span>
#include <vector>

namespace {

using Catch::Matchers::WithinAbs;

constexpr float rate = 48'000.0F;

// 20 ms at 48 kHz.
constexpr std::size_t ramp_frames = 960;

constexpr irama::ParamDescriptor gain{
    .id = irama::ParamId{1},
    .name = "Gain",
    .unit = irama::ParamUnit::decibels,
    .min = -60.0F,
    .max = 12.0F,
    .default_value = -12.0F,
    .smoothing_seconds = 0.02F,
};

constexpr irama::ParamDescriptor cutoff{
    .id = irama::ParamId{2},
    .name = "Cutoff",
    .unit = irama::ParamUnit::hertz,
    .min = 20.0F,
    .max = 20'000.0F,
    .default_value = 20'000.0F,
    .scale = irama::ParamScale::logarithmic,
    .smoothing_seconds = 0.02F,
};

constexpr irama::ParamDescriptor attack{
    .id = irama::ParamId{3},
    .name = "Attack",
    .unit = irama::ParamUnit::seconds,
    .min = 0.001F,
    .max = 10.0F,
    .default_value = 0.002F,
    .scale = irama::ParamScale::logarithmic,
};

// Renders the smoother's next values and moves it on past them, as an instrument does, inside a
// render scope so the real-time checks watch it.
void render(irama::ParamSmoother& smoother, std::span<float> values) IRAMA_NONBLOCKING {
    const irama::RenderScope scope;
    smoother.render(values);
    smoother.advance(values.size());
}

[[nodiscard]] std::vector<float> render(irama::ParamSmoother& smoother, std::size_t frames) {
    std::vector<float> values(frames);
    render(smoother, values);
    return values;
}

// The value at frame `index` of a ramp of `ramp_frames`, counting from 0, from the normalised value
// `from` to `to`.
[[nodiscard]] double ramp_value(const irama::ParamDescriptor& descriptor, double from, double to,
                                std::size_t index) {
    const double progress = static_cast<double>(index + 1) / static_cast<double>(ramp_frames);
    return static_cast<double>(
        descriptor.from_normalized(static_cast<float>(from + ((to - from) * progress))));
}

} // namespace

TEST_CASE("a smoother starts at its parameter's default, at rest", "[param]") {
    irama::ParamSmoother smoother{cutoff, rate};
    REQUIRE(smoother.value() == 20'000.0F);
    REQUIRE(smoother.target() == 20'000.0F);
    REQUIRE_FALSE(smoother.ramping());
    REQUIRE(render(smoother, 10) == std::vector<float>(10, 20'000.0F));
}

TEST_CASE("a smoothed parameter ramps linearly over exactly its smoothing time", "[param]") {
    irama::ParamSmoother smoother{gain, rate};
    smoother.set(0.0F, true);
    REQUIRE(smoother.target() == 0.0F);
    REQUIRE(smoother.ramping());
    const auto values = render(smoother, ramp_frames + 100);

    const auto from = static_cast<double>(gain.to_normalized(-12.0F));
    const auto to = static_cast<double>(gain.to_normalized(0.0F));
    for (std::size_t index = 0; index + 1 < ramp_frames; ++index) {
        REQUIRE_THAT(values.at(index), WithinAbs(ramp_value(gain, from, to, index), 1e-5));
    }
    // The ramp reaches its target on its last frame exactly, and not before.
    REQUIRE(values.at(ramp_frames - 2) < 0.0F);
    REQUIRE(std::ranges::all_of(std::span{values}.subspan(ramp_frames - 1),
                                [](float value) { return value == 0.0F; }));
    REQUIRE(smoother.value() == 0.0F);
    REQUIRE_FALSE(smoother.ramping());
}

TEST_CASE("a logarithmic parameter ramps evenly in octaves", "[param]") {
    irama::ParamSmoother smoother{cutoff, rate};
    smoother.set(20.0F, true);
    const auto values = render(smoother, ramp_frames);
    // 20 kHz to 20 Hz, the same distance each frame in the normalised domain, so the same number
    // of octaves each frame.
    for (std::size_t index = 0; index + 1 < ramp_frames; ++index) {
        const double expected = ramp_value(cutoff, 1.0, 0.0, index);
        REQUIRE_THAT(values.at(index), WithinAbs(expected, expected * 1e-4));
    }
    REQUIRE(values.back() == 20.0F);
    REQUIRE_THAT(values.at((ramp_frames / 2) - 1), WithinAbs(632.456, 0.1));
}

TEST_CASE("a new target restarts the ramp from the current value", "[param]") {
    irama::ParamSmoother smoother{gain, rate};
    smoother.set(12.0F, true);
    static_cast<void>(render(smoother, 300));
    const float current = smoother.value();
    REQUIRE(current > -12.0F);
    REQUIRE(current < 12.0F);

    smoother.set(-60.0F, true);
    const auto values = render(smoother, ramp_frames);
    const auto from = static_cast<double>(gain.to_normalized(current));
    for (std::size_t index = 0; index + 1 < ramp_frames; ++index) {
        REQUIRE_THAT(values.at(index), WithinAbs(ramp_value(gain, from, 0.0, index), 1e-5));
    }
    REQUIRE(values.back() == -60.0F);
}

TEST_CASE("a value set without smoothing jumps to it", "[param]") {
    irama::ParamSmoother smoother{gain, rate};
    smoother.set(6.0F, false);
    REQUIRE(smoother.value() == 6.0F);
    REQUIRE_FALSE(smoother.ramping());
    REQUIRE(render(smoother, 5) == std::vector<float>(5, 6.0F));

    // A jump also ends a ramp under way.
    smoother.set(-6.0F, true);
    static_cast<void>(render(smoother, 10));
    smoother.set(1.0F, false);
    REQUIRE(render(smoother, 5) == std::vector<float>(5, 1.0F));
}

TEST_CASE("a parameter with no smoothing time always switches at once", "[param]") {
    irama::ParamSmoother smoother{attack, rate};
    smoother.set(0.5F, true);
    REQUIRE(smoother.value() == 0.5F);
    REQUIRE_FALSE(smoother.ramping());
    REQUIRE(render(smoother, 3) == std::vector<float>(3, 0.5F));
}

TEST_CASE("rendering a smoother's values doesn't move it on", "[param]") {
    irama::ParamSmoother smoother{gain, rate};
    smoother.set(0.0F, true);
    std::vector<float> first(100);
    std::vector<float> again(100);
    {
        const irama::RenderScope scope;
        smoother.render(first);
        smoother.render(again);
    }
    REQUIRE(first == again);

    irama::ParamSmoother whole{gain, rate};
    whole.set(0.0F, true);
    const auto at_once = render(whole, 400);
    std::vector<float> in_parts = render(smoother, 150);
    const auto rest = render(smoother, 250);
    in_parts.insert(in_parts.end(), rest.begin(), rest.end());
    REQUIRE(in_parts == at_once);
}
