// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include <irama/engine/base/allocator.hpp>

#include "counting_allocator.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

TEST_CASE("the allocation entry point allocates from the allocator it is given", "[base]") {
    irama::test::CountingAllocator counting;
    void* block = irama::allocate(counting.allocator(), 24, 8);

    REQUIRE(block != nullptr);
    REQUIRE(counting.allocations() == 1);
    REQUIRE(counting.last().pointer == block);
    REQUIRE(counting.last().size == 24);
    REQUIRE(counting.last().alignment == 8);
    irama::deallocate(counting.allocator(), block, 24, 8);
}

TEST_CASE("the allocation entry point frees through the allocator it is given", "[base]") {
    irama::test::CountingAllocator counting;
    void* block = irama::allocate(counting.allocator(), 24, 8);
    irama::deallocate(counting.allocator(), block, 24, 8);

    REQUIRE(counting.frees() == 1);
    REQUIRE(counting.mismatched_frees() == 0);
    REQUIRE(counting.live().empty());
}

TEST_CASE("the allocation entry point returns null when the allocator has no memory", "[base]") {
    irama::test::CountingAllocator counting;
    counting.fail_next();
    REQUIRE(irama::allocate(counting.allocator(), 24, 8) == nullptr);
}

TEST_CASE("the default allocator returns blocks of the size and alignment asked for", "[base]") {
    const std::size_t alignment = GENERATE(1U, 2U, 8U, 16U, 64U, 4096U);
    const std::size_t size = GENERATE(1U, 3U, 100U, 5000U);
    const irama::Allocator allocator = irama::default_allocator();
    void* block = irama::allocate(allocator, size, alignment);

    REQUIRE(block != nullptr);
    REQUIRE(std::bit_cast<std::uintptr_t>(block) % alignment == 0);
    // Under AddressSanitizer, a block shorter than its size fails here.
    std::memset(block, 0xA5, size);
    irama::deallocate(allocator, block, size, alignment);
}

TEST_CASE("the default allocator returns null for a size it can't round to its alignment",
          "[base]") {
    REQUIRE(irama::allocate(irama::default_allocator(), std::numeric_limits<std::size_t>::max(),
                            64) == nullptr);
}
