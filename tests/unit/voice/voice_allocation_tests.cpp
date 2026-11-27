// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include <irama/engine/voice/voice_allocation.hpp>

#include <irama/engine/base/pcg32.hpp>
#include <irama/engine/base/render_scope.hpp>
#include <irama/engine/voice/instrument.hpp>
#include <irama/engine/voice/voice.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace {

constexpr float rate = 48'000.0F;

// The default attack and decay at 48 kHz, after which a held voice sustains.
constexpr std::size_t attack_and_decay_frames = 96 + 14'400;

// Renders the voices' next frames together, as the player does, and throws the frames away.
void advance(std::span<irama::Voice> voices, irama::Instrument& instrument, std::size_t frames) {
    std::vector<float> out(irama::Instrument::max_block_frames * 2);
    std::vector<float> scratch(irama::Instrument::max_block_frames *
                               irama::Voice::scratch_per_frame);
    while (frames > 0) {
        const std::size_t length = std::min(frames, irama::Instrument::max_block_frames);
        const irama::RenderScope scope;
        const irama::InstrumentBlock block = instrument.render(length);
        for (irama::Voice& voice : voices) {
            voice.render(std::span{out}.first(length * 2), scratch, block);
        }
        frames -= length;
    }
}

void start(irama::Voice& voice, const irama::Instrument& instrument, float velocity,
           std::uint64_t order) {
    voice.start(57, velocity, instrument, irama::Pcg32{}, order);
}

} // namespace

TEST_CASE("a note-on takes the first free voice", "[voice]") {
    const irama::Instrument instrument{rate};
    std::array<irama::Voice, 3> voices;
    REQUIRE(irama::choose_voice(voices) == 0);
    start(voices.at(0), instrument, 1.0F, 0);
    start(voices.at(2), instrument, 1.0F, 1);
    REQUIRE(irama::choose_voice(voices) == 1);
}

TEST_CASE("with no voice free, a note-on steals the oldest voice in its release", "[voice]") {
    irama::Instrument instrument{rate};
    std::array<irama::Voice, 4> voices;
    // The oldest voice, and the quietest, is held, so neither counts against a voice in its
    // release.
    start(voices.at(0), instrument, 0.1F, 0);
    start(voices.at(1), instrument, 1.0F, 3);
    start(voices.at(2), instrument, 1.0F, 1);
    start(voices.at(3), instrument, 1.0F, 2);
    advance(voices, instrument, 1'000);
    // The newer note is released first, so the oldest in its release isn't the longest released.
    voices.at(1).release(instrument);
    advance(voices, instrument, 100);
    voices.at(2).release(instrument);
    advance(voices, instrument, 100);
    REQUIRE(irama::choose_voice(voices) == 2);
}

TEST_CASE("with no voice free or in its release, a note-on steals the quietest voice", "[voice]") {
    irama::Instrument instrument{rate};

    SECTION("by its velocity's gain") {
        std::array<irama::Voice, 4> voices;
        start(voices.at(0), instrument, 1.0F, 0);
        start(voices.at(1), instrument, 0.7F, 1);
        start(voices.at(2), instrument, 0.5F, 3);
        start(voices.at(3), instrument, 0.6F, 2);
        advance(voices, instrument, 1'000);
        REQUIRE(irama::choose_voice(voices) == 2);
    }

    SECTION("by its envelope's level") {
        // A loud note sustaining at 70 %, and a newer one, played harder, still in its attack.
        std::array<irama::Voice, 2> voices;
        start(voices.at(0), instrument, 0.9F, 0);
        advance(std::span{voices}.first(1), instrument, attack_and_decay_frames);
        start(voices.at(1), instrument, 1.0F, 1);
        advance(voices, instrument, 10);
        REQUIRE(voices.at(1).level() < voices.at(0).level());
        REQUIRE(irama::choose_voice(voices) == 1);
    }
}

TEST_CASE("of equally quiet voices, a note-on steals the oldest", "[voice]") {
    irama::Instrument instrument{rate};
    std::array<irama::Voice, 3> voices;
    start(voices.at(0), instrument, 0.8F, 2);
    start(voices.at(1), instrument, 0.8F, 0);
    start(voices.at(2), instrument, 0.8F, 1);
    REQUIRE(irama::choose_voice(voices) == 1);
    advance(voices, instrument, 1'000);
    REQUIRE(voices.at(0).level() == voices.at(1).level());
    REQUIRE(irama::choose_voice(voices) == 1);
}

TEST_CASE("a voice's level is its envelope's times its velocity's gain", "[voice]") {
    irama::Instrument instrument{rate};
    std::array<irama::Voice, 1> voices;
    start(voices.at(0), instrument, 0.5F, 0);
    REQUIRE(voices.at(0).level() == 0.0F);
    advance(voices, instrument, attack_and_decay_frames);
    // The sustain level, 70 %, times 0.5², at full velocity sensitivity.
    REQUIRE(voices.at(0).level() == 0.7F * 0.25F);
}

TEST_CASE("a stolen voice fades out in the first free fade slot", "[voice]") {
    irama::Instrument instrument{rate};
    std::array<irama::Voice, 3> slots;
    REQUIRE(irama::choose_fade_slot(slots) == 0);
    start(slots.at(0), instrument, 1.0F, 0);
    slots.at(0).fade_out();
    REQUIRE(irama::choose_fade_slot(slots) == 1);
    // A fade that has ended frees its slot.
    advance(slots, instrument, 240);
    REQUIRE(irama::choose_fade_slot(slots) == 0);
}

TEST_CASE("with every fade slot busy, a steal cuts the fade nearest its end", "[voice]") {
    irama::Instrument instrument{rate};
    std::array<irama::Voice, 3> slots;
    for (std::size_t index = 0; index < slots.size(); ++index) {
        start(slots.at(index), instrument, 1.0F, index);
    }
    slots.at(1).fade_out();
    advance(slots, instrument, 100);
    slots.at(2).fade_out();
    advance(slots, instrument, 50);
    slots.at(0).fade_out();
    advance(slots, instrument, 10);
    REQUIRE(slots.at(1).fade_frames_left() == 80);
    REQUIRE(irama::choose_fade_slot(slots) == 1);
}
