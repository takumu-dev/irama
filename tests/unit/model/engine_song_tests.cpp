// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include <irama/engine/model/engine_song.hpp>

#include "counting_allocator.hpp"
#include <irama/engine/base/allocator.hpp>
#include <irama/engine/base/nonblocking.hpp>
#include <irama/engine/base/render_scope.hpp>
#include <irama/engine/model/change_set.hpp>
#include <irama/engine/model/instrument_part.hpp>
#include <irama/engine/model/pattern_part.hpp>
#include <irama/engine/model/song_part.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace {

using irama::EngineSong;

[[nodiscard]] irama::InstrumentPartPtr instrument(const irama::Allocator& allocator, float value) {
    const std::array values{value, 1.0F};
    auto part = irama::InstrumentPart::create(allocator, values);
    REQUIRE(part.has_value());
    return std::move(*part);
}

[[nodiscard]] irama::PatternPartPtr pattern(const irama::Allocator& allocator, std::size_t lines) {
    const std::vector<irama::NoteCell> cells(lines * 4);
    auto part = irama::PatternPart::create(allocator, lines, 4, cells);
    REQUIRE(part.has_value());
    return std::move(*part);
}

[[nodiscard]] irama::SongPartPtr song(const irama::Allocator& allocator, std::uint64_t seed) {
    auto part = irama::SongPart::create(allocator, {.seed = seed});
    REQUIRE(part.has_value());
    return std::move(*part);
}

// A change set of a song part, patterns 0 and 999, and instruments 01 and FE.
[[nodiscard]] irama::ChangeSetPtr whole_song(const irama::Allocator& allocator) {
    auto changes = irama::ChangeSet::create(allocator, {.patterns = 2, .instruments = 2});
    REQUIRE(changes.has_value());
    (*changes)->replace_song(song(allocator, 7));
    (*changes)->replace_pattern(0, pattern(allocator, 64));
    (*changes)->replace_pattern(999, pattern(allocator, 16));
    (*changes)->replace_instrument(1, instrument(allocator, 0.5F));
    (*changes)->replace_instrument(0xFE, instrument(allocator, 0.25F));
    return std::move(*changes);
}

// A change set of one instrument entry.
[[nodiscard]] irama::ChangeSetPtr one_instrument(std::size_t number,
                                                 irama::InstrumentPartPtr part) {
    auto changes = irama::ChangeSet::create(irama::default_allocator(), {.instruments = 1});
    REQUIRE(changes.has_value());
    (*changes)->replace_instrument(number, std::move(part));
    return std::move(*changes);
}

// A change set of one pattern entry.
[[nodiscard]] irama::ChangeSetPtr one_pattern(std::size_t number, irama::PatternPartPtr part) {
    auto changes = irama::ChangeSet::create(irama::default_allocator(), {.patterns = 1});
    REQUIRE(changes.has_value());
    (*changes)->replace_pattern(number, std::move(part));
    return std::move(*changes);
}

// A change set of the song part alone.
[[nodiscard]] irama::ChangeSetPtr one_song(irama::SongPartPtr part) {
    auto changes = irama::ChangeSet::create(irama::default_allocator());
    REQUIRE(changes.has_value());
    (*changes)->replace_song(std::move(part));
    return std::move(*changes);
}

// Applies inside a render scope, as the audio thread does, so the real-time checks watch it.
void apply(EngineSong& engine_song, irama::ChangeSet& changes) IRAMA_NONBLOCKING {
    const irama::RenderScope scope;
    engine_song.apply(changes);
}

} // namespace

TEST_CASE("a new EngineSong is empty", "[model]") {
    const auto engine_song = std::make_unique<EngineSong>();
    CHECK(engine_song->song() == nullptr);
    CHECK(engine_song->pattern(0) == nullptr);
    CHECK(engine_song->pattern(999) == nullptr);
    CHECK(engine_song->instrument(1) == nullptr);
    CHECK(engine_song->instrument(0xFE) == nullptr);
    CHECK(*engine_song == *std::make_unique<EngineSong>());
}

TEST_CASE("a change set puts its parts in their entries, and takes the parts they held",
          "[model]") {
    const irama::Allocator allocator = irama::default_allocator();
    const auto engine_song = std::make_unique<EngineSong>();
    const auto first = whole_song(allocator);
    const auto first_instruments = first->instruments();
    const irama::InstrumentPart* first_instrument = first_instruments.front().part.get();
    apply(*engine_song, *first);
    CHECK(engine_song->song()->seed() == 7);
    CHECK(engine_song->pattern(0)->lines() == 64);
    CHECK(engine_song->pattern(999)->lines() == 16);
    CHECK(engine_song->pattern(1) == nullptr);
    CHECK(engine_song->instrument(1) == first_instrument);
    CHECK(engine_song->instrument(0xFE)->values().front() == 0.25F);
    // The entries were empty, and so is what the change set took from them.
    CHECK(first->song() == nullptr);
    CHECK(first->patterns().front().part == nullptr);
    CHECK(first->instruments().back().part == nullptr);

    const irama::ChangeSetPtr emptied = one_pattern(999, nullptr);
    const irama::ChangeSetPtr replaced = one_instrument(1, instrument(allocator, 0.75F));
    apply(*engine_song, *emptied);
    apply(*engine_song, *replaced);
    CHECK(engine_song->pattern(999) == nullptr);
    CHECK(engine_song->instrument(1)->values().front() == 0.75F);
    CHECK(engine_song->song()->seed() == 7);
    CHECK(emptied->patterns().front().part->lines() == 16);
    CHECK(replaced->instruments().front().part.get() == first_instrument);
}

TEST_CASE("EngineSongs are equal when every entry holds an equal part or none", "[model]") {
    const irama::Allocator allocator = irama::default_allocator();
    const auto left = std::make_unique<EngineSong>();
    const auto right = std::make_unique<EngineSong>();
    apply(*left, *whole_song(allocator));
    CHECK_FALSE(*left == *right);
    apply(*right, *whole_song(allocator));
    CHECK(*left == *right);

    apply(*right, *one_instrument(0xFE, instrument(allocator, 0.3F)));
    CHECK_FALSE(*left == *right);
    apply(*right, *one_instrument(0xFE, instrument(allocator, 0.25F)));
    CHECK(*left == *right);
    apply(*right, *one_pattern(500, pattern(allocator, 1)));
    CHECK_FALSE(*left == *right);
    apply(*left, *one_pattern(500, pattern(allocator, 1)));
    CHECK(*left == *right);
    apply(*left, *one_song(song(allocator, 8)));
    CHECK_FALSE(*left == *right);
}

TEST_CASE("every part an EngineSong held is freed through its allocator", "[model]") {
    irama::test::CountingAllocator counting;
    {
        const irama::Allocator allocator = counting.allocator();
        const auto engine_song = std::make_unique<EngineSong>();
        apply(*engine_song, *whole_song(allocator));
        const std::size_t song_blocks = counting.live().size();
        auto replaced = whole_song(allocator);
        apply(*engine_song, *replaced);
        // The change set holds the first song's parts, and the EngineSong the second's.
        REQUIRE(counting.live().size() > song_blocks);
        replaced.reset();
        REQUIRE(counting.live().size() == song_blocks);
    }
    REQUIRE(counting.frees() == counting.allocations());
    REQUIRE(counting.mismatched_frees() == 0);
    REQUIRE(counting.live().empty());
}
