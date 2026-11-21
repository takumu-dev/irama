// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include <irama/engine/voice/voice.hpp>

#include <irama/engine/base/nonblocking.hpp>
#include <irama/engine/base/render_scope.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <cstddef>
#include <numbers>
#include <span>
#include <vector>

namespace {

using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

constexpr float rate = 48'000.0F;

// The default envelope at 48 kHz: the attack and decay together, and the release.
constexpr std::size_t attack_and_decay_frames = 96 + 14'400;
constexpr std::size_t release_frames = 9'600;

// Renders as the player does, inside a render scope, so the real-time checks watch the voice.
void render(irama::Voice& voice, std::span<float> out, std::span<float> scratch) IRAMA_NONBLOCKING {
    const irama::RenderScope scope;
    voice.render(out, scratch);
}

// Renders the voice's next frames, interleaved stereo, onto silence.
[[nodiscard]] std::vector<float> render(irama::Voice& voice, std::size_t frames) {
    std::vector<float> out(frames * 2);
    std::vector<float> scratch(out.size());
    render(voice, out, scratch);
    return out;
}

} // namespace

TEST_CASE("a voice sounds its note's sine at -15 dB on each channel at full velocity", "[voice]") {
    irama::Voice voice;
    voice.start(57, 1.0F, rate);
    const auto out = render(voice, attack_and_decay_frames + 480);

    // −12 dB, less 3 dB for the centre of a constant-power pan, at the sustain level.
    const double gain = std::pow(10.0, -12.0 / 20.0) * std::cos(std::numbers::pi / 4.0) * 0.7;
    for (std::size_t frame = attack_and_decay_frames; frame < out.size() / 2; ++frame) {
        const double expected =
            gain * std::sin(2.0 * std::numbers::pi * 440.0 * static_cast<double>(frame) / 48'000.0);
        REQUIRE_THAT(out.at(2 * frame), WithinAbs(expected, 1e-4));
        REQUIRE(out.at((2 * frame) + 1) == out.at(2 * frame));
    }
}

TEST_CASE("a voice's level follows the square of its velocity", "[voice]") {
    irama::Voice loud;
    irama::Voice quiet;
    loud.start(60, 1.0F, rate);
    quiet.start(60, 0.5F, rate);
    const auto loud_out = render(loud, 1'000);
    const auto quiet_out = render(quiet, 1'000);
    for (std::size_t index = 0; index < loud_out.size(); ++index) {
        if (loud_out.at(index) != 0.0F) {
            REQUIRE_THAT(quiet_out.at(index) / loud_out.at(index), WithinRel(0.25F, 1e-5F));
        }
    }
}

TEST_CASE("a voice adds to what is already there", "[voice]") {
    irama::Voice voice;
    irama::Voice twin;
    voice.start(57, 1.0F, rate);
    twin.start(57, 1.0F, rate);
    std::vector<float> out(512, 0.25F);
    std::vector<float> scratch(out.size());
    render(voice, out, scratch);
    const auto alone = render(twin, 256);
    for (std::size_t index = 0; index < out.size(); ++index) {
        REQUIRE(out.at(index) == 0.25F + alone.at(index));
    }
}

TEST_CASE("a voice is held until it is released, and free once its release ends", "[voice]") {
    irama::Voice voice;
    REQUIRE_FALSE(voice.active());
    voice.start(48, 0.8F, rate);
    REQUIRE(voice.held());
    REQUIRE(voice.note() == 48);
    static_cast<void>(render(voice, 1'000));

    voice.release();
    REQUIRE(voice.active());
    REQUIRE_FALSE(voice.held());
    static_cast<void>(render(voice, release_frames - 1));
    REQUIRE(voice.active());
    static_cast<void>(render(voice, 1));
    REQUIRE_FALSE(voice.active());
    REQUIRE(render(voice, 100) == std::vector<float>(200, 0.0F));
}
