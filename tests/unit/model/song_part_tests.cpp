// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include <irama/engine/model/song_part.hpp>

#include "counting_allocator.hpp"
#include <irama/engine/base/allocator.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <utility>

namespace {

constexpr std::array<std::uint8_t, 3> columns{2, 1, 12};
constexpr std::array<std::uint16_t, 4> sequence{0, 7, 7, 999};

[[nodiscard]] irama::SongDescription description() {
    return {
        .bpm100 = 12'345,
        .lpb = 8,
        .beats_per_bar = 3,
        .ticks_per_line = 6,
        .seed = 42,
        .note_columns = columns,
        .sequence = sequence,
    };
}

[[nodiscard]] irama::SongPartPtr make(const irama::SongDescription& song) {
    auto part = irama::SongPart::create(irama::default_allocator(), song);
    REQUIRE(part.has_value());
    return std::move(*part);
}

} // namespace

TEST_CASE("a song part built from no description holds the new song", "[model]") {
    const auto part = make({});
    CHECK(part->bpm100() == 12'000);
    CHECK(part->lpb() == 4);
    CHECK(part->beats_per_bar() == 4);
    CHECK(part->ticks_per_line() == 12);
    CHECK(part->seed() == 0x6972'616d'6131);
    CHECK(std::ranges::equal(part->note_columns(), std::array<std::uint8_t, 4>{1, 1, 1, 1}));
    CHECK(std::ranges::equal(part->sequence(), std::array<std::uint16_t, 1>{0}));
}

TEST_CASE("a song part holds what it is built from", "[model]") {
    const auto part = make(description());
    CHECK(part->bpm100() == 12'345);
    CHECK(part->lpb() == 8);
    CHECK(part->beats_per_bar() == 3);
    CHECK(part->ticks_per_line() == 6);
    CHECK(part->seed() == 42);
    CHECK(std::ranges::equal(part->note_columns(), columns));
    CHECK(std::ranges::equal(part->sequence(), sequence));
}

TEST_CASE("song parts are equal when they hold the same song", "[model]") {
    CHECK(*make(description()) == *make(description()));
    CHECK(*make({}) == *make({}));

    auto tempo = description();
    tempo.bpm100 = 12'346;
    CHECK_FALSE(*make(tempo) == *make(description()));
    auto seed = description();
    seed.seed = 43;
    CHECK_FALSE(*make(seed) == *make(description()));
    auto shorter = description();
    shorter.sequence = std::span{sequence}.first(3);
    CHECK_FALSE(*make(shorter) == *make(description()));
    constexpr std::array<std::uint8_t, 3> other_columns{2, 2, 12};
    auto wider = description();
    wider.note_columns = other_columns;
    CHECK_FALSE(*make(wider) == *make(description()));
}

TEST_CASE("a song part is freed through the allocator it was built with", "[model]") {
    irama::test::CountingAllocator counting;
    {
        const auto part = irama::SongPart::create(counting.allocator(), description());
        REQUIRE(part.has_value());
        REQUIRE(counting.allocations() == 1);
    }
    REQUIRE(counting.frees() == 1);
    REQUIRE(counting.mismatched_frees() == 0);
    REQUIRE(counting.live().empty());
}

TEST_CASE("a song part reports running out of memory", "[model]") {
    irama::test::CountingAllocator counting;
    counting.fail_next();
    const auto part = irama::SongPart::create(counting.allocator(), description());
    REQUIRE(part.error() == irama::AllocationError::out_of_memory);
}
