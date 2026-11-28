// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include <irama/engine/model/pattern_part.hpp>

#include "counting_allocator.hpp"
#include <irama/engine/base/allocator.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

namespace {

using irama::NoteCell;

constexpr NoteCell c4{.note = 48, .instrument = 1, .volume = 0x40, .pan = 0x40, .delay = 0};
constexpr NoteCell off{.note = NoteCell::off};

// Three lines of two columns, with a note on the first line and an OFF on the third.
[[nodiscard]] std::vector<NoteCell> cells() {
    std::vector<NoteCell> pattern(6);
    pattern.at(1) = c4;
    pattern.at(4) = off;
    return pattern;
}

[[nodiscard]] irama::PatternPartPtr make(std::size_t lines, std::size_t columns,
                                         const std::vector<NoteCell>& pattern) {
    auto part = irama::PatternPart::create(irama::default_allocator(), lines, columns, pattern);
    REQUIRE(part.has_value());
    return std::move(*part);
}

} // namespace

TEST_CASE("a pattern part holds its lines of cells, line by line", "[model]") {
    const auto part = make(3, 2, cells());
    REQUIRE(part->lines() == 3);
    REQUIRE(part->columns() == 2);
    REQUIRE(std::ranges::equal(part->cells(), cells()));
    REQUIRE(part->line(0).front() == NoteCell{});
    REQUIRE(part->line(0).back() == c4);
    REQUIRE(part->line(2).front() == off);
}

TEST_CASE("pattern parts are equal when they hold the same cells in the same shape", "[model]") {
    CHECK(*make(3, 2, cells()) == *make(3, 2, cells()));
    CHECK_FALSE(*make(3, 2, cells()) == *make(2, 3, cells()));
    auto moved = cells();
    moved.at(5) = off;
    CHECK_FALSE(*make(3, 2, cells()) == *make(3, 2, moved));
}

TEST_CASE("a pattern part frees every block through the allocator it was built with", "[model]") {
    irama::test::CountingAllocator counting;
    {
        const auto part = irama::PatternPart::create(counting.allocator(), 3, 2, cells());
        REQUIRE(part.has_value());
        REQUIRE(counting.allocations() == 2);
    }
    REQUIRE(counting.frees() == 2);
    REQUIRE(counting.mismatched_frees() == 0);
    REQUIRE(counting.live().empty());
}

TEST_CASE("a pattern part that runs out of memory leaks nothing", "[model]") {
    for (std::size_t failing = 1; failing <= 2; ++failing) {
        irama::test::CountingAllocator counting;
        counting.fail_at(failing);
        const auto part = irama::PatternPart::create(counting.allocator(), 3, 2, cells());
        REQUIRE(part.error() == irama::AllocationError::out_of_memory);
        REQUIRE(counting.live().empty());
        REQUIRE(counting.mismatched_frees() == 0);
    }
}
