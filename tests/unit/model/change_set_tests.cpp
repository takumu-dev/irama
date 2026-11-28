// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include <irama/engine/model/change_set.hpp>

#include "counting_allocator.hpp"
#include <irama/engine/base/allocator.hpp>
#include <irama/engine/model/instrument_part.hpp>
#include <irama/engine/model/pattern_part.hpp>
#include <irama/engine/model/song_part.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <utility>

namespace {

constexpr std::array values{1.0F, 2.0F};
constexpr std::array<irama::NoteCell, 1> one_cell{};

[[nodiscard]] irama::ChangeSetPtr make(const irama::Allocator& allocator,
                                       const irama::ChangeSetSize& size) {
    auto changes = irama::ChangeSet::create(allocator, size);
    REQUIRE(changes.has_value());
    return std::move(*changes);
}

[[nodiscard]] irama::InstrumentPartPtr instrument(const irama::Allocator& allocator) {
    auto part = irama::InstrumentPart::create(allocator, values);
    REQUIRE(part.has_value());
    return std::move(*part);
}

[[nodiscard]] irama::PatternPartPtr pattern(const irama::Allocator& allocator) {
    auto part = irama::PatternPart::create(allocator, 1, 1, one_cell);
    REQUIRE(part.has_value());
    return std::move(*part);
}

[[nodiscard]] irama::SongPartPtr song(const irama::Allocator& allocator) {
    auto part = irama::SongPart::create(allocator);
    REQUIRE(part.has_value());
    return std::move(*part);
}

} // namespace

TEST_CASE("a new change set replaces nothing", "[model]") {
    const auto changes = make(irama::default_allocator(), {.patterns = 2, .instruments = 3});
    CHECK(changes->empty());
    CHECK_FALSE(changes->replaces_song());
    CHECK(changes->song() == nullptr);
    CHECK(changes->patterns().empty());
    CHECK(changes->instruments().empty());
    CHECK(changes->slot_map().empty());
    CHECK(changes->track_map().empty());
}

TEST_CASE("a change set holds its parts in the order they were added", "[model]") {
    const irama::Allocator allocator = irama::default_allocator();
    const auto changes = make(allocator, {.patterns = 2, .instruments = 2});
    changes->replace_instrument(0xFE, instrument(allocator));
    changes->replace_pattern(999, nullptr);
    changes->replace_instrument(1, nullptr);
    changes->replace_pattern(3, pattern(allocator));
    changes->replace_song(song(allocator));

    CHECK_FALSE(changes->empty());
    CHECK(changes->replaces_song());
    CHECK(changes->song() != nullptr);
    REQUIRE(changes->patterns().size() == 2);
    CHECK(changes->patterns().front().number == 999);
    CHECK(changes->patterns().front().part == nullptr);
    CHECK(changes->patterns().back().number == 3);
    CHECK(changes->patterns().back().part != nullptr);
    REQUIRE(changes->instruments().size() == 2);
    CHECK(changes->instruments().front().number == 0xFE);
    CHECK(changes->instruments().front().part != nullptr);
    CHECK(changes->instruments().back().number == 1);
    CHECK(changes->instruments().back().part == nullptr);
}

TEST_CASE("a change set has position maps of the lengths asked for", "[model]") {
    const auto changes = make(irama::default_allocator(), {.slots = 3, .tracks = 2});
    REQUIRE(changes->slot_map().size() == 3);
    REQUIRE(changes->track_map().size() == 2);
    CHECK_FALSE(changes->empty());
    changes->slot_map().subspan(1, 1).front() = irama::ChangeSet::gone;
    CHECK(std::as_const(*changes).slot_map().subspan(1, 1).front() == irama::ChangeSet::gone);
}

TEST_CASE("a change set frees every block of its own and of its parts", "[model]") {
    irama::test::CountingAllocator counting;
    {
        const irama::Allocator allocator = counting.allocator();
        const auto changes =
            make(allocator, {.patterns = 1, .instruments = 1, .slots = 4, .tracks = 1});
        changes->replace_song(song(allocator));
        changes->replace_pattern(0, pattern(allocator));
        changes->replace_instrument(1, instrument(allocator));
    }
    REQUIRE(counting.allocations() > 0);
    REQUIRE(counting.frees() == counting.allocations());
    REQUIRE(counting.mismatched_frees() == 0);
    REQUIRE(counting.live().empty());
}

TEST_CASE("a change set that runs out of memory leaks nothing", "[model]") {
    constexpr irama::ChangeSetSize size{.patterns = 1, .instruments = 1, .slots = 1, .tracks = 1};
    std::size_t failures = 0;
    for (std::size_t failing = 1;; ++failing) {
        irama::test::CountingAllocator counting;
        counting.fail_at(failing);
        const auto changes = irama::ChangeSet::create(counting.allocator(), size);
        if (changes.has_value()) {
            break;
        }
        REQUIRE(changes.error() == irama::AllocationError::out_of_memory);
        REQUIRE(counting.live().empty());
        REQUIRE(counting.mismatched_frees() == 0);
        ++failures;
    }
    REQUIRE(failures == 5);
}
