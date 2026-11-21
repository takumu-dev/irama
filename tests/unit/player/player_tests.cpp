// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include <irama/engine/player/player.hpp>

#include "counting_allocator.hpp"
#include <irama/engine/base/pcg32.hpp>
#include <irama/engine/base/render_scope.hpp>
#include <irama/engine/param/param_descriptor.hpp>
#include <irama/engine/voice/instrument.hpp>
#include <irama/engine/voice/voice.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <utility>
#include <vector>

namespace {

constexpr std::uint32_t rate = 48'000;

// The default release at 48 kHz.
constexpr std::size_t release_frames = 9'600;

[[nodiscard]] irama::PlayerPtr make_player(std::size_t max_voices = 32) {
    auto player = irama::Player::create({.sample_rate = rate, .max_voices = max_voices});
    REQUIRE(player.has_value());
    return std::move(*player);
}

// Renders the player's next frames, interleaved stereo.
[[nodiscard]] std::vector<float> render(irama::Player& player, std::size_t frames) {
    std::vector<float> out(frames * 2);
    player.render(out);
    return out;
}

// Renders the player's next frames in calls of the lengths given, one after another, repeating
// them until all the frames are rendered.
[[nodiscard]] std::vector<float> render_split(irama::Player& player, std::size_t frames,
                                              const std::vector<std::size_t>& lengths) {
    std::vector<float> out(frames * 2);
    std::span<float> rest{out};
    for (std::size_t call = 0; !rest.empty(); ++call) {
        const std::size_t length = std::min(lengths.at(call % lengths.size()) * 2, rest.size());
        player.render(rest.first(length));
        rest = rest.subspan(length);
    }
    return out;
}

// A change of the gain while notes sound, and of the release before they are released.
void send_changes(irama::Player& player) {
    REQUIRE(player.set_parameter(9'000, irama::InstrumentParam::gain, -6.0F));
    REQUIRE(player.set_parameter(9'500, irama::InstrumentParam::release, 0.5F));
}

// Chords that overlap, with a note-off, notes sharing a frame, parameter changes and a release-all,
// to be sent to players that render them in different ways.
void send_chords(irama::Player& player) {
    REQUIRE(player.note_on(0, 48, 0.8F));
    REQUIRE(player.note_on(1'000, 52, 0.7F));
    REQUIRE(player.note_on(1'000, 55, 0.6F));
    REQUIRE(player.note_off(5'000, 52));
    REQUIRE(player.note_on(7'777, 60, 1.0F));
    REQUIRE(player.note_on(7'777, 64, 0.9F));
    send_changes(player);
    REQUIRE(player.release_all(12'000));
}

// Values other than the defaults for the gain, the pan and the sustain level.
void send_values(irama::Player& player, std::uint64_t frame) {
    REQUIRE(player.set_parameter(frame, irama::InstrumentParam::gain, 0.0F));
    REQUIRE(player.set_parameter(frame, irama::InstrumentParam::pan, 0.75F));
    REQUIRE(player.set_parameter(frame, irama::InstrumentParam::sustain, 40.0F));
}

// A player of a seed playing two notes of noise alone, overlapping.
[[nodiscard]] irama::PlayerPtr make_noise_player(std::uint64_t seed) {
    auto player = irama::Player::create({.sample_rate = rate, .seed = seed});
    REQUIRE(player.has_value());
    REQUIRE((*player)->set_parameter(0, irama::InstrumentParam::osc1_level, 0.0F));
    REQUIRE((*player)->set_parameter(0, irama::InstrumentParam::noise_level, 100.0F));
    REQUIRE((*player)->note_on(0, 57, 1.0F));
    REQUIRE((*player)->note_on(500, 60, 1.0F));
    return std::move(*player);
}

// A player of noise alone, panned hard left, starting `notes` notes of A-4 at frame 0.
[[nodiscard]] irama::PlayerPtr make_noise_chord(int notes) {
    auto player = make_player();
    REQUIRE(player->set_parameter(0, irama::InstrumentParam::osc1_level, 0.0F));
    REQUIRE(player->set_parameter(0, irama::InstrumentParam::noise_level, 100.0F));
    REQUIRE(player->set_parameter(0, irama::InstrumentParam::pan, -1.0F));
    for (int note = 0; note < notes; ++note) {
        REQUIRE(player->note_on(0, 57, 1.0F));
    }
    return player;
}

[[nodiscard]] bool silent(std::span<const float> samples) {
    return std::ranges::all_of(samples, [](float sample) { return sample == 0.0F; });
}

} // namespace

TEST_CASE("a player renders the same messages alike however its calls are split", "[player]") {
    constexpr std::size_t frames = 24'000;
    const auto whole = make_player();
    send_chords(*whole);
    const auto once = render(*whole, frames);
    REQUIRE_FALSE(silent(once));

    const auto blocks = make_player();
    send_chords(*blocks);
    REQUIRE(render_split(*blocks, frames, {256}) == once);

    const auto ragged = make_player();
    send_chords(*ragged);
    REQUIRE(render_split(*ragged, frames, {1, 7, 0, 255, 257, 1'000, 4'096}) == once);
}

TEST_CASE("a player frees every block it allocated, with its size and alignment", "[player]") {
    irama::test::CountingAllocator counting;
    {
        auto player = irama::Player::create(
            {.sample_rate = rate, .max_voices = 32, .allocator = counting.allocator()});
        REQUIRE(player.has_value());
        REQUIRE(counting.allocations() > 0);
        const std::size_t allocated = counting.allocations();
        REQUIRE((*player)->note_on(0, 57, 1.0F));
        static_cast<void>(render(**player, 1'000));
        REQUIRE(counting.allocations() == allocated);
    }
    REQUIRE(counting.frees() == counting.allocations());
    REQUIRE(counting.mismatched_frees() == 0);
    REQUIRE(counting.live().empty());
}

TEST_CASE("a player whose allocator runs out of memory isn't created, and leaks nothing",
          "[player]") {
    std::size_t failures = 0;
    for (std::size_t failing = 1;; ++failing) {
        irama::test::CountingAllocator counting;
        counting.fail_at(failing);
        const auto player = irama::Player::create(
            {.sample_rate = rate, .max_voices = 32, .allocator = counting.allocator()});
        if (player.has_value()) {
            break;
        }
        REQUIRE(player.error() == irama::CreateError::out_of_memory);
        REQUIRE(counting.mismatched_frees() == 0);
        REQUIRE(counting.live().empty());
        ++failures;
    }
    REQUIRE(failures > 0);
}

TEST_CASE("a player applies a message exactly at its frame", "[player]") {
    const auto player = make_player();
    REQUIRE(player->note_on(100, 57, 1.0F));
    const auto out = render(*player, 300);
    // The note's sine starts at phase 0, so its first frame is silent too.
    constexpr std::size_t first_sound = std::size_t{101} * 2;
    REQUIRE(silent(std::span{out}.first(first_sound)));
    REQUIRE(out.at(first_sound) != 0.0F);
}

TEST_CASE("a message at a frame already past applies at the first frame of the next render",
          "[player]") {
    const auto late = make_player();
    static_cast<void>(render(*late, 1'000));
    REQUIRE(late->note_on(500, 57, 1.0F));
    const auto late_out = render(*late, 2'000);

    const auto immediate = make_player();
    static_cast<void>(render(*immediate, 1'000));
    REQUIRE(immediate->note_on(0, 57, 1.0F));

    const auto on_time = make_player();
    REQUIRE(on_time->note_on(1'000, 57, 1.0F));
    static_cast<void>(render(*on_time, 1'000));
    const auto on_time_out = render(*on_time, 2'000);

    REQUIRE_FALSE(silent(on_time_out));
    REQUIRE(late_out == on_time_out);
    REQUIRE(render(*immediate, 2'000) == on_time_out);
}

TEST_CASE("a player writes what its voices sum to", "[player]") {
    const auto player = make_player();
    REQUIRE(player->note_on(0, 57, 1.0F));
    REQUIRE(player->note_on(0, 64, 0.5F));
    REQUIRE(player->set_parameter(1'000, irama::InstrumentParam::pan, -0.5F));
    const auto out = render(*player, 2'000);

    // The same voices, reading the same instrument, which glides to its new pan at frame 1000,
    // each with the noise generator the player's default seed gives it.
    irama::Instrument instrument{static_cast<float>(rate)};
    irama::Pcg32 generator{irama::PlayerConfig{}.seed, 0};
    irama::Voice first;
    irama::Voice second;
    first.start(57, 1.0F, instrument, generator.split());
    second.start(64, 0.5F, instrument, generator.split());
    std::vector<float> expected(out.size());
    std::vector<float> scratch(irama::Instrument::max_block_frames *
                               irama::Voice::scratch_per_frame);
    std::span<float> rest{expected};
    for (std::size_t frame = 0; frame < 2'000;) {
        if (frame == 1'000) {
            instrument.set(irama::find_param(irama::instrument_params, irama::InstrumentParam::pan)
                               .value_or(irama::instrument_params.size()),
                           -0.5F, true);
        }
        const auto frames = std::min<std::size_t>(
            {irama::Instrument::max_block_frames, (frame < 1'000 ? 1'000 : 2'000) - frame});
        const irama::RenderScope scope;
        const auto block = instrument.render(frames);
        first.render(rest.first(frames * 2), scratch, block);
        second.render(rest.first(frames * 2), scratch, block);
        rest = rest.subspan(frames * 2);
        frame += frames;
    }
    REQUIRE(out == expected);
}

TEST_CASE("a player given every value by set-parameter at frame 0 renders the same as one created "
          "with them",
          "[player]") {
    constexpr std::size_t frames = 24'000;
    const auto created = make_player();
    send_chords(*created);
    const auto expected = render(*created, frames);

    const auto replayed = make_player();
    for (const irama::ParamDescriptor& param : irama::instrument_params) {
        REQUIRE(replayed->set_parameter(0, param.id, param.default_value));
    }
    send_chords(*replayed);
    REQUIRE(render(*replayed, frames) == expected);
}

TEST_CASE("a value set while no voice sounds jumps to it as if set at frame 0", "[player]") {
    constexpr std::size_t frames = 12'000;
    const auto at_start = make_player();
    send_values(*at_start, 0);
    REQUIRE(at_start->note_on(12'000, 57, 0.9F));
    static_cast<void>(render(*at_start, 12'000));
    const auto expected = render(*at_start, frames);

    // A note sounds and ends before the values are set, at the frame of the next note.
    const auto later = make_player();
    REQUIRE(later->note_on(0, 60, 1.0F));
    REQUIRE(later->note_off(100, 60));
    send_values(*later, 12'000);
    REQUIRE(later->note_on(12'000, 57, 0.9F));
    static_cast<void>(render(*later, 12'000));
    // The first note's release has ended.
    REQUIRE(later->active_voices() == 0);
    REQUIRE(render(*later, frames) == expected);
}

TEST_CASE("a value set while a voice sounds glides there", "[player]") {
    const auto jumped = make_player();
    REQUIRE(jumped->set_parameter(0, irama::InstrumentParam::pan, -1.0F));
    REQUIRE(jumped->note_on(0, 57, 1.0F));

    const auto glided = make_player();
    REQUIRE(glided->note_on(0, 57, 1.0F));
    REQUIRE(glided->set_parameter(0, irama::InstrumentParam::pan, -1.0F));

    // The glide takes 20 ms, 960 frames, to reach the pan the other player jumped to.
    const auto jumped_out = render(*jumped, 2'000);
    const auto glided_out = render(*glided, 2'000);
    constexpr std::size_t arrived = std::size_t{959} * 2;
    REQUIRE(glided_out.at(arrived - 1) != 0.0F);
    REQUIRE(std::ranges::equal(std::span{glided_out}.subspan(arrived),
                               std::span{jumped_out}.subspan(arrived)));
    REQUIRE(jumped_out.at(arrived + 1) == 0.0F);
}

TEST_CASE("a set-parameter clamps its value to the parameter's range", "[player]") {
    const auto clamped = make_player();
    REQUIRE(clamped->set_parameter(0, irama::InstrumentParam::gain, 100.0F));
    REQUIRE(clamped->set_parameter(0, irama::InstrumentParam::sustain,
                                   -std::numeric_limits<float>::infinity()));
    send_chords(*clamped);

    const auto bounds = make_player();
    REQUIRE(bounds->set_parameter(0, irama::InstrumentParam::gain, 12.0F));
    REQUIRE(bounds->set_parameter(0, irama::InstrumentParam::sustain, 0.0F));
    send_chords(*bounds);
    REQUIRE(render(*clamped, 20'000) == render(*bounds, 20'000));
}

TEST_CASE("a player counts the voices sounding at the end of each render call", "[player]") {
    const auto player = make_player();
    REQUIRE(player->active_voices() == 0);
    REQUIRE(player->note_on(0, 48, 1.0F));
    REQUIRE(player->note_on(0, 52, 1.0F));
    REQUIRE(player->note_on(0, 55, 1.0F));
    REQUIRE(player->active_voices() == 0);
    static_cast<void>(render(*player, 10));
    REQUIRE(player->active_voices() == 3);

    REQUIRE(player->note_off(100, 52));
    static_cast<void>(render(*player, 90 + release_frames - 1));
    REQUIRE(player->active_voices() == 3);
    static_cast<void>(render(*player, 1));
    REQUIRE(player->active_voices() == 2);

    // Frames never go backwards, so this one is already past, and applies at once.
    REQUIRE(player->release_all(100));
    static_cast<void>(render(*player, release_frames));
    REQUIRE(player->active_voices() == 0);
}

TEST_CASE("a note-off releases every held voice playing its note", "[player]") {
    const auto player = make_player();
    REQUIRE(player->note_on(0, 57, 1.0F));
    REQUIRE(player->note_on(100, 57, 0.5F));
    REQUIRE(player->note_on(100, 60, 0.5F));
    REQUIRE(player->note_off(1'000, 57));
    static_cast<void>(render(*player, 1'000 + release_frames));
    REQUIRE(player->active_voices() == 1);
}

TEST_CASE("with no voice free, a note-on is ignored", "[player]") {
    const auto crowded = make_player(2);
    REQUIRE(crowded->note_on(0, 48, 1.0F));
    REQUIRE(crowded->note_on(0, 52, 1.0F));
    REQUIRE(crowded->note_on(10, 55, 1.0F));
    const auto crowded_out = render(*crowded, 1'000);
    REQUIRE(crowded->active_voices() == 2);

    const auto pair = make_player(2);
    REQUIRE(pair->note_on(0, 48, 1.0F));
    REQUIRE(pair->note_on(0, 52, 1.0F));
    REQUIRE(render(*pair, 1'000) == crowded_out);
}

TEST_CASE("a released voice is free again for the next note-on", "[player]") {
    const auto player = make_player(1);
    REQUIRE(player->note_on(0, 48, 1.0F));
    REQUIRE(player->note_off(10, 48));
    REQUIRE(player->note_on(10 + release_frames, 52, 1.0F));
    static_cast<void>(render(*player, 20 + release_frames));
    REQUIRE(player->active_voices() == 1);
}

TEST_CASE("a full queue refuses a message, and takes messages again once rendered", "[player]") {
    const auto player = make_player();
    for (std::size_t index = 0; index < irama::Player::queue_capacity; ++index) {
        REQUIRE(player->release_all(0));
    }
    REQUIRE_FALSE(player->note_on(0, 57, 1.0F));
    static_cast<void>(render(*player, 1));
    REQUIRE(player->active_voices() == 0);
    REQUIRE(player->note_on(0, 57, 1.0F));
    static_cast<void>(render(*player, 1));
    REQUIRE(player->active_voices() == 1);
}

TEST_CASE("a player renders no frames when asked for none", "[player]") {
    const auto player = make_player();
    REQUIRE(player->note_on(0, 57, 1.0F));
    player->render({});
    REQUIRE(player->active_voices() == 0);
    const auto out = render(*player, 300);
    REQUIRE_FALSE(silent(out));
}

TEST_CASE("two players with the same seed render the same noise", "[player]") {
    const auto first = make_noise_player(1'234);
    const auto second = make_noise_player(1'234);
    const auto out = render(*first, 2'000);
    REQUIRE_FALSE(silent(out));
    REQUIRE(render(*second, 2'000) == out);
}

TEST_CASE("players with different seeds render different noise", "[player]") {
    const auto first = make_noise_player(1);
    const auto second = make_noise_player(2);
    REQUIRE(render(*first, 1'000) != render(*second, 1'000));
}

TEST_CASE("each note-on gives its voice a noise stream of its own", "[player]") {
    // Two notes at once on the same pitch, with nothing but noise, would sound as one note twice
    // as loud if their noise were the same.
    const auto one_note = make_noise_chord(1);
    const auto two_notes = make_noise_chord(2);
    const auto one = render(*one_note, 1'000);
    const auto two = render(*two_notes, 1'000);
    std::vector<float> doubled(one.size());
    std::ranges::transform(one, doubled.begin(), [](float sample) { return 2.0F * sample; });
    REQUIRE(two != doubled);
}
