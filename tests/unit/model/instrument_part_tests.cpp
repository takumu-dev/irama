// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include <irama/engine/model/instrument_part.hpp>

#include "counting_allocator.hpp"
#include <irama/engine/base/allocator.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <utility>

namespace {

constexpr std::array values{2.0F, -12.0F, 0.5F};

[[nodiscard]] irama::InstrumentPartPtr make(std::span<const float> part_values) {
    auto part = irama::InstrumentPart::create(irama::default_allocator(), part_values);
    REQUIRE(part.has_value());
    return std::move(*part);
}

} // namespace

TEST_CASE("an instrument part holds its values", "[model]") {
    REQUIRE(std::ranges::equal(make(values)->values(), values));
}

TEST_CASE("instrument parts are equal when they hold the same values", "[model]") {
    constexpr std::array other{2.0F, -11.0F, 0.5F};
    CHECK(*make(values) == *make(values));
    CHECK_FALSE(*make(values) == *make(other));
    CHECK_FALSE(*make(values) == *make(std::span{values}.first(2)));
}

TEST_CASE("an instrument part frees every block through the allocator it was built with",
          "[model]") {
    irama::test::CountingAllocator counting;
    {
        const auto part = irama::InstrumentPart::create(counting.allocator(), values);
        REQUIRE(part.has_value());
        REQUIRE(counting.allocations() == 2);
    }
    REQUIRE(counting.frees() == 2);
    REQUIRE(counting.mismatched_frees() == 0);
    REQUIRE(counting.live().empty());
}

TEST_CASE("an instrument part that runs out of memory leaks nothing", "[model]") {
    for (std::size_t failing = 1; failing <= 2; ++failing) {
        irama::test::CountingAllocator counting;
        counting.fail_at(failing);
        const auto part = irama::InstrumentPart::create(counting.allocator(), values);
        REQUIRE(part.error() == irama::AllocationError::out_of_memory);
        REQUIRE(counting.live().empty());
    }
}
