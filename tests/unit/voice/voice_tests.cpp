// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include <irama/engine/voice/voice.hpp>

#include <irama/engine/base/nonblocking.hpp>
#include <irama/engine/base/render_scope.hpp>
#include <irama/engine/param/param_descriptor.hpp>
#include <irama/engine/param/param_id.hpp>
#include <irama/engine/voice/instrument.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
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

// Renders as the player does, inside a render scope, so the real-time checks watch the voice: the
// instrument's values for a block, then the voice.
void render_block(irama::Voice& voice, irama::Instrument& instrument, std::span<float> out,
                  std::span<float> scratch) IRAMA_NONBLOCKING {
    const irama::RenderScope scope;
    const irama::InstrumentBlock block = instrument.render(out.size() / 2);
    voice.render(out, scratch, block);
}

// Adds the voice's next frames into `out`, interleaved stereo, a block at a time.
void render(irama::Voice& voice, irama::Instrument& instrument, std::span<float> out) {
    std::vector<float> scratch(irama::Instrument::max_block_frames * 2);
    while (!out.empty()) {
        const std::size_t length = std::min(out.size(), scratch.size());
        render_block(voice, instrument, out.first(length), scratch);
        out = out.subspan(length);
    }
}

// Renders the voice's next frames, interleaved stereo, onto silence.
[[nodiscard]] std::vector<float> render(irama::Voice& voice, irama::Instrument& instrument,
                                        std::size_t frames) {
    std::vector<float> out(frames * 2);
    render(voice, instrument, out);
    return out;
}

// The index of one of the instrument's parameters, or one past the table's end, which any use
// of it catches, for an ID it hasn't.
[[nodiscard]] std::size_t index_of(irama::ParamId id) {
    return irama::find_param(irama::instrument_params, id)
        .value_or(irama::instrument_params.size());
}

} // namespace

TEST_CASE("a voice sounds its note's sine at -15 dB on each channel at full velocity", "[voice]") {
    irama::Instrument instrument{rate};
    irama::Voice voice;
    voice.start(57, 1.0F, instrument);
    const auto out = render(voice, instrument, attack_and_decay_frames + 480);

    // −12 dB, less 3 dB for the centre of a constant-power pan, at the sustain level.
    const double gain = std::pow(10.0, -12.0 / 20.0) * std::cos(std::numbers::pi / 4.0) * 0.7;
    for (std::size_t frame = attack_and_decay_frames; frame < out.size() / 2; ++frame) {
        const double expected =
            gain * std::sin(2.0 * std::numbers::pi * 440.0 * static_cast<double>(frame) / 48'000.0);
        REQUIRE_THAT(out.at(2 * frame), WithinAbs(expected, 1e-4));
        REQUIRE(out.at((2 * frame) + 1) == out.at(2 * frame));
    }
}

TEST_CASE("a voice's level follows its velocity's gain", "[voice]") {
    const float sensitivity = GENERATE(100.0F, 50.0F, 0.0F);
    irama::Instrument instrument{rate};
    instrument.set(index_of(irama::InstrumentParam::velocity_sensitivity), sensitivity, false);
    irama::Instrument twin{rate};
    twin.set(index_of(irama::InstrumentParam::velocity_sensitivity), sensitivity, false);
    irama::Voice loud;
    irama::Voice quiet;
    loud.start(60, 1.0F, instrument);
    quiet.start(60, 0.5F, twin);
    const auto loud_out = render(loud, instrument, 1'000);
    const auto quiet_out = render(quiet, twin, 1'000);
    // 1 − s · (1 − v²), for the sensitivity s.
    const float expected = 1.0F - ((sensitivity / 100.0F) * 0.75F);
    for (std::size_t index = 0; index < loud_out.size(); ++index) {
        if (loud_out.at(index) != 0.0F) {
            REQUIRE_THAT(quiet_out.at(index) / loud_out.at(index), WithinRel(expected, 1e-5F));
        }
    }
}

TEST_CASE("a voice adds to what is already there", "[voice]") {
    irama::Instrument instrument{rate};
    irama::Instrument twin_instrument{rate};
    irama::Voice voice;
    irama::Voice twin;
    voice.start(57, 1.0F, instrument);
    twin.start(57, 1.0F, twin_instrument);
    std::vector<float> out(512, 0.25F);
    render(voice, instrument, out);
    const auto alone = render(twin, twin_instrument, 256);
    for (std::size_t index = 0; index < out.size(); ++index) {
        REQUIRE(out.at(index) == 0.25F + alone.at(index));
    }
}

TEST_CASE("a voice follows the instrument's sustain level and pan while it sounds", "[voice]") {
    irama::Instrument instrument{rate};
    irama::Voice voice;
    voice.start(57, 1.0F, instrument);
    static_cast<void>(render(voice, instrument, attack_and_decay_frames));
    instrument.set(index_of(irama::InstrumentParam::sustain), 35.0F, true);
    instrument.set(index_of(irama::InstrumentParam::pan), -1.0F, true);
    // Once both have glided there, over 20 ms, the voice is at half its level, all on the left.
    static_cast<void>(render(voice, instrument, 960));
    const auto out = render(voice, instrument, 480);
    const double gain = std::pow(10.0, -12.0 / 20.0) * 0.35;
    for (std::size_t frame = 0; frame < out.size() / 2; ++frame) {
        const double expected =
            gain * std::sin(2.0 * std::numbers::pi * 440.0 *
                            static_cast<double>(attack_and_decay_frames + 960 + frame) / 48'000.0);
        REQUIRE_THAT(out.at(2 * frame), WithinAbs(expected, 1e-4));
        REQUIRE(out.at((2 * frame) + 1) == 0.0F);
    }
}

TEST_CASE("a voice is held until it is released, and free once its release ends", "[voice]") {
    irama::Instrument instrument{rate};
    irama::Voice voice;
    REQUIRE_FALSE(voice.active());
    voice.start(48, 0.8F, instrument);
    REQUIRE(voice.held());
    REQUIRE(voice.note() == 48);
    static_cast<void>(render(voice, instrument, 1'000));

    voice.release(instrument);
    REQUIRE(voice.active());
    REQUIRE_FALSE(voice.held());
    static_cast<void>(render(voice, instrument, release_frames - 1));
    REQUIRE(voice.active());
    static_cast<void>(render(voice, instrument, 1));
    REQUIRE_FALSE(voice.active());
    REQUIRE(render(voice, instrument, 100) == std::vector<float>(200, 0.0F));
}
