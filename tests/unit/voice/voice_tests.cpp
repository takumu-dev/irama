// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include <irama/engine/voice/voice.hpp>

#include <irama/engine/base/nonblocking.hpp>
#include <irama/engine/base/pcg32.hpp>
#include <irama/engine/base/render_scope.hpp>
#include <irama/engine/param/param_descriptor.hpp>
#include <irama/engine/param/param_id.hpp>
#include <irama/engine/source/noise.hpp>
#include <irama/engine/source/oscillator.hpp>
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
    std::vector<float> scratch(irama::Instrument::max_block_frames *
                               irama::Voice::scratch_per_frame);
    while (!out.empty()) {
        const std::size_t length = std::min(out.size(), irama::Instrument::max_block_frames * 2);
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

// Sets the instrument's waveform, at once.
void set_waveform(irama::Instrument& instrument, irama::Waveform waveform) {
    instrument.set(index_of(irama::InstrumentParam::osc1_waveform),
                   static_cast<float>(static_cast<int>(waveform)), false);
}

// An instrument playing one waveform.
[[nodiscard]] irama::Instrument instrument_playing(irama::Waveform waveform, float sample_rate) {
    irama::Instrument instrument{sample_rate};
    set_waveform(instrument, waveform);
    return instrument;
}

// −12 dB, less 3 dB for the centre of a constant-power pan, at the sustain level.
const double sustain_gain = std::pow(10.0, -12.0 / 20.0) * std::cos(std::numbers::pi / 4.0) * 0.7;

} // namespace

TEST_CASE("a voice sounds its note's sine at -15 dB on each channel at full velocity", "[voice]") {
    auto instrument = instrument_playing(irama::Waveform::sine, rate);
    irama::Voice voice;
    voice.start(57, 1.0F, instrument, irama::Pcg32{});
    const auto out = render(voice, instrument, attack_and_decay_frames + 480);

    const double gain = sustain_gain;
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
    loud.start(60, 1.0F, instrument, irama::Pcg32{});
    quiet.start(60, 0.5F, twin, irama::Pcg32{});
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
    voice.start(57, 1.0F, instrument, irama::Pcg32{});
    twin.start(57, 1.0F, twin_instrument, irama::Pcg32{});
    std::vector<float> out(512, 0.25F);
    render(voice, instrument, out);
    const auto alone = render(twin, twin_instrument, 256);
    for (std::size_t index = 0; index < out.size(); ++index) {
        REQUIRE(out.at(index) == 0.25F + alone.at(index));
    }
}

TEST_CASE("a voice follows the instrument's sustain level and pan while it sounds", "[voice]") {
    auto instrument = instrument_playing(irama::Waveform::sine, rate);
    irama::Voice voice;
    voice.start(57, 1.0F, instrument, irama::Pcg32{});
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
    voice.start(48, 0.8F, instrument, irama::Pcg32{});
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

TEST_CASE("a waveform change crossfades linearly over exactly 5 ms", "[voice]") {
    const float sample_rate = GENERATE(48'000.0F, 44'100.0F);
    // 5 ms, rounded to the nearest frame.
    const std::size_t crossfade = sample_rate == 48'000.0F ? 240 : 221;
    constexpr std::size_t before = 1'000;

    auto instrument = instrument_playing(irama::Waveform::sine, sample_rate);
    auto sine_instrument = instrument_playing(irama::Waveform::sine, sample_rate);
    auto saw_instrument = instrument_playing(irama::Waveform::saw, sample_rate);
    irama::Voice voice;
    irama::Voice sine;
    irama::Voice saw;
    voice.start(57, 1.0F, instrument, irama::Pcg32{});
    sine.start(57, 1.0F, sine_instrument, irama::Pcg32{});
    saw.start(57, 1.0F, saw_instrument, irama::Pcg32{});

    const auto out_before = render(voice, instrument, before);
    REQUIRE(out_before == render(sine, sine_instrument, before));
    static_cast<void>(render(saw, saw_instrument, before));
    set_waveform(instrument, irama::Waveform::saw);
    const auto out = render(voice, instrument, crossfade + 100);
    const auto old_out = render(sine, sine_instrument, crossfade + 100);
    const auto new_out = render(saw, saw_instrument, crossfade + 100);

    // Frame i of the crossfade, counting from 0, is (i + 1) / n of the way to the new waveform, so
    // its last frame is the new waveform alone.
    for (std::size_t frame = 0; frame < crossfade; ++frame) {
        const double progress = static_cast<double>(frame + 1) / static_cast<double>(crossfade);
        for (std::size_t channel = 0; channel < 2; ++channel) {
            const std::size_t index = (2 * frame) + channel;
            const double expected = ((1.0 - progress) * static_cast<double>(old_out.at(index))) +
                                    (progress * static_cast<double>(new_out.at(index)));
            REQUIRE_THAT(out.at(index), WithinAbs(expected, 1e-6));
        }
    }
    REQUIRE(out.at(2 * (crossfade - 2)) != new_out.at(2 * (crossfade - 2)));
    REQUIRE(std::ranges::equal(std::span{out}.subspan(2 * (crossfade - 1)),
                               std::span{new_out}.subspan(2 * (crossfade - 1))));
}

TEST_CASE("a waveform change during a crossfade crossfades again from the waveform fading in",
          "[voice]") {
    auto instrument = instrument_playing(irama::Waveform::sine, rate);
    auto saw_instrument = instrument_playing(irama::Waveform::saw, rate);
    auto pulse_instrument = instrument_playing(irama::Waveform::pulse, rate);
    irama::Voice voice;
    irama::Voice saw;
    irama::Voice pulse;
    voice.start(57, 1.0F, instrument, irama::Pcg32{});
    saw.start(57, 1.0F, saw_instrument, irama::Pcg32{});
    pulse.start(57, 1.0F, pulse_instrument, irama::Pcg32{});
    static_cast<void>(render(voice, instrument, 1'000));
    static_cast<void>(render(saw, saw_instrument, 1'100));
    static_cast<void>(render(pulse, pulse_instrument, 1'100));
    set_waveform(instrument, irama::Waveform::saw);
    static_cast<void>(render(voice, instrument, 100));
    set_waveform(instrument, irama::Waveform::pulse);

    const auto out = render(voice, instrument, 300);
    const auto saw_out = render(saw, saw_instrument, 300);
    const auto pulse_out = render(pulse, pulse_instrument, 300);
    for (std::size_t frame = 0; frame < 240; ++frame) {
        const double progress = static_cast<double>(frame + 1) / 240.0;
        const double expected = ((1.0 - progress) * static_cast<double>(saw_out.at(2 * frame))) +
                                (progress * static_cast<double>(pulse_out.at(2 * frame)));
        REQUIRE_THAT(out.at(2 * frame), WithinAbs(expected, 1e-6));
    }
    REQUIRE(std::ranges::equal(std::span{out}.subspan(std::size_t{2} * 239),
                               std::span{pulse_out}.subspan(std::size_t{2} * 239)));
}

TEST_CASE("a voice started after a waveform change plays the new waveform with no crossfade",
          "[voice]") {
    irama::Instrument instrument{rate};
    set_waveform(instrument, irama::Waveform::triangle);
    auto triangle_instrument = instrument_playing(irama::Waveform::triangle, rate);
    irama::Voice voice;
    irama::Voice triangle;
    voice.start(60, 1.0F, instrument, irama::Pcg32{});
    triangle.start(60, 1.0F, triangle_instrument, irama::Pcg32{});
    REQUIRE(render(voice, instrument, 500) == render(triangle, triangle_instrument, 500));
}

TEST_CASE("a voice mixes its noise at the noise level", "[voice]") {
    irama::Instrument instrument{rate};
    instrument.set(index_of(irama::InstrumentParam::osc1_level), 0.0F, false);
    instrument.set(index_of(irama::InstrumentParam::noise_level), 50.0F, false);
    irama::Voice voice;
    voice.start(57, 1.0F, instrument, irama::Pcg32{5, 6});
    const auto out = render(voice, instrument, attack_and_decay_frames + 480);

    irama::Noise noise;
    noise.start(irama::Pcg32{5, 6});
    std::vector<float> expected(attack_and_decay_frames + 480);
    {
        const irama::RenderScope scope;
        noise.render(expected);
    }
    for (std::size_t frame = attack_and_decay_frames; frame < expected.size(); ++frame) {
        REQUIRE_THAT(out.at(2 * frame),
                     WithinAbs(0.5 * sustain_gain * static_cast<double>(expected.at(frame)), 1e-6));
        REQUIRE(out.at((2 * frame) + 1) == out.at(2 * frame));
    }
}

TEST_CASE("each voice's noise follows the generator it was started with", "[voice]") {
    irama::Instrument instrument{rate};
    instrument.set(index_of(irama::InstrumentParam::noise_level), 100.0F, false);
    irama::Instrument twin{rate};
    twin.set(index_of(irama::InstrumentParam::noise_level), 100.0F, false);
    irama::Voice first;
    irama::Voice second;
    first.start(57, 1.0F, instrument, irama::Pcg32{1, 1});
    second.start(57, 1.0F, twin, irama::Pcg32{1, 2});
    REQUIRE(render(first, instrument, 500) != render(second, twin, 500));
}

TEST_CASE("a voice plays its oscillator at the oscillator's level", "[voice]") {
    auto full = instrument_playing(irama::Waveform::saw, rate);
    auto quarter = instrument_playing(irama::Waveform::saw, rate);
    quarter.set(index_of(irama::InstrumentParam::osc1_level), 25.0F, false);
    irama::Voice loud;
    irama::Voice quiet;
    loud.start(50, 1.0F, full, irama::Pcg32{});
    quiet.start(50, 1.0F, quarter, irama::Pcg32{});
    const auto loud_out = render(loud, full, 1'000);
    const auto quiet_out = render(quiet, quarter, 1'000);
    for (std::size_t index = 0; index < loud_out.size(); ++index) {
        REQUIRE_THAT(quiet_out.at(index),
                     WithinAbs(0.25 * static_cast<double>(loud_out.at(index)), 1e-7));
    }
}

TEST_CASE("a voice tuned by the coarse and fine offsets plays at the tuned frequency", "[voice]") {
    auto tuned = instrument_playing(irama::Waveform::saw, rate);
    tuned.set(index_of(irama::InstrumentParam::osc1_coarse), -12.0F, false);
    auto octave_down = instrument_playing(irama::Waveform::saw, rate);
    irama::Voice voice;
    irama::Voice reference;
    voice.start(69, 1.0F, tuned, irama::Pcg32{});
    reference.start(57, 1.0F, octave_down, irama::Pcg32{});
    REQUIRE(render(voice, tuned, 1'000) == render(reference, octave_down, 1'000));
}

TEST_CASE("a coarse step during a note jumps in pitch with a continuous phase", "[voice]") {
    auto instrument = instrument_playing(irama::Waveform::sine, rate);
    irama::Voice voice;
    voice.start(57, 1.0F, instrument, irama::Pcg32{});
    static_cast<void>(render(voice, instrument, attack_and_decay_frames));
    instrument.set(index_of(irama::InstrumentParam::osc1_coarse), 12.0F, true);
    const auto out = render(voice, instrument, 480);
    for (std::size_t frame = 0; frame < out.size() / 2; ++frame) {
        const double phase = (static_cast<double>(attack_and_decay_frames) * 440.0 / 48'000.0) +
                             (static_cast<double>(frame) * 880.0 / 48'000.0);
        const double expected = sustain_gain * std::sin(2.0 * std::numbers::pi * phase);
        REQUIRE_THAT(out.at(2 * frame), WithinAbs(expected, 1e-4));
    }
}
