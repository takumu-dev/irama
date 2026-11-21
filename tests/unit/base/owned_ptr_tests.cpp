// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include <irama/engine/base/owned_ptr.hpp>

#include "counting_allocator.hpp"
#include <irama/engine/base/allocator.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>

namespace {

// Aligned beyond what the default allocator gives by default, and filling its alignment, since
// MSVC warns about a type padded out to one.
struct alignas(32) Wide {
    std::array<std::byte, 64> bytes{};
};

// Counts its constructions and destructions in the counters it is given.
class Counted {
  public:
    Counted(int& constructed, int& destroyed) : destroyed_{&destroyed} { ++constructed; }

    Counted(const Counted&) = delete;
    Counted& operator=(const Counted&) = delete;
    Counted(Counted&&) = delete;
    Counted& operator=(Counted&&) = delete;

    ~Counted() { ++*destroyed_; }

  private:
    int* destroyed_;
};

} // namespace

TEST_CASE("make_owned constructs the object with the arguments it is given", "[base]") {
    irama::test::CountingAllocator counting;
    const auto owned =
        irama::make_owned<std::array<int, 3>>(counting.allocator(), std::array{1, 2, 3});

    REQUIRE(owned.has_value());
    REQUIRE(**owned == std::array{1, 2, 3});
}

TEST_CASE("an owned pointer frees its object with the size and alignment it was allocated with",
          "[base]") {
    irama::test::CountingAllocator counting;
    auto owned = irama::make_owned<Wide>(counting.allocator());

    REQUIRE(owned.has_value());
    REQUIRE(counting.last().size == sizeof(Wide));
    REQUIRE(counting.last().alignment == alignof(Wide));
    owned->reset();
    REQUIRE(counting.frees() == 1);
    REQUIRE(counting.mismatched_frees() == 0);
    REQUIRE(counting.live().empty());
}

TEST_CASE("an owned pointer destroys its object when it frees it", "[base]") {
    irama::test::CountingAllocator counting;
    int constructed = 0;
    int destroyed = 0;
    {
        const auto owned = irama::make_owned<Counted>(counting.allocator(), constructed, destroyed);
        REQUIRE(owned.has_value());
        REQUIRE(constructed == 1);
        REQUIRE(destroyed == 0);
    }
    REQUIRE(destroyed == 1);
    REQUIRE(counting.live().empty());
}

TEST_CASE("make_owned reports running out of memory, and constructs nothing", "[base]") {
    irama::test::CountingAllocator counting;
    counting.fail_next();
    int constructed = 0;
    int destroyed = 0;
    const auto owned = irama::make_owned<Counted>(counting.allocator(), constructed, destroyed);

    REQUIRE(owned.error() == irama::AllocationError::out_of_memory);
    REQUIRE(constructed == 0);
}
