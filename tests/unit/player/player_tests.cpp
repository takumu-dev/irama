// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include <irama/engine/player/player.hpp>

#include "counting_allocator.hpp"
#include <irama/engine/base/render_scope.hpp>
#include <irama/engine/voice/voice.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
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

// Chords that overlap, with a note-off, notes sharing a frame and a release-all, to be sent to
// players that render them in different ways.
void send_chords(irama::Player& player) {
    REQUIRE(player.note_on(0, 48, 0.8F));
    REQUIRE(player.note_on(1'000, 52, 0.7F));
    REQUIRE(player.note_on(1'000, 55, 0.6F));
    REQUIRE(player.note_off(5'000, 52));
    REQUIRE(player.note_on(7'777, 60, 1.0F));
    REQUIRE(player.note_on(7'777, 64, 0.9F));
    REQUIRE(player.release_all(12'000));
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
    const auto out = render(*player, 2'000);

    irama::Voice first;
    irama::Voice second;
    first.start(57, 1.0F, static_cast<float>(rate));
    second.start(64, 0.5F, static_cast<float>(rate));
    std::vector<float> expected(out.size());
    std::vector<float> scratch(out.size());
    {
        const irama::RenderScope scope;
        first.render(expected, scratch);
        second.render(expected, scratch);
    }
    REQUIRE(out == expected);
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
