// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include <irama/engine/base/owned_array.hpp>

#include "counting_allocator.hpp"
#include <irama/engine/base/allocator.hpp>
#include <irama/engine/base/nonblocking.hpp>
#include <irama/engine/base/render_scope.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <utility>

namespace {

// Aligned beyond what the default allocator gives by default, and filling its alignment, since
// MSVC warns about a type padded out to one.
struct alignas(32) Wide {
    std::array<std::byte, 32> bytes{};
};

// Counts its destructions in the counter it is given.
struct Counted {
    int* destroyed = nullptr;

    Counted() = default;
    Counted(const Counted&) = delete;
    Counted& operator=(const Counted&) = delete;
    Counted(Counted&&) = delete;
    Counted& operator=(Counted&&) = delete;

    ~Counted() {
        if (destroyed != nullptr) {
            ++*destroyed;
        }
    }
};

// Stands in for a render entry point: the real-time guard, where it is compiled in, and
// RealtimeSanitizer, where that is on, stop the test at the first allocation inside it.
template <typename Body> void render(Body body) IRAMA_NONBLOCKING {
    const irama::RenderScope scope;
    body();
}

} // namespace

TEST_CASE("an owned array holds its length of value-initialised elements", "[base]") {
    irama::test::CountingAllocator counting;
    const auto array = irama::OwnedArray<int>::create(counting.allocator(), 5);

    REQUIRE(array.has_value());
    REQUIRE(array->size() == 5);
    REQUIRE(std::ranges::all_of(*array, [](int element) { return element == 0; }));
}

TEST_CASE("an owned array frees its elements with the size and alignment they were allocated with",
          "[base]") {
    irama::test::CountingAllocator counting;
    {
        const auto array = irama::OwnedArray<Wide>::create(counting.allocator(), 7);
        REQUIRE(array.has_value());
        REQUIRE(counting.last().size == 7 * sizeof(Wide));
        REQUIRE(counting.last().alignment == alignof(Wide));
    }
    REQUIRE(counting.frees() == 1);
    REQUIRE(counting.mismatched_frees() == 0);
    REQUIRE(counting.live().empty());
}

TEST_CASE("an owned array destroys each of its elements when it frees them", "[base]") {
    irama::test::CountingAllocator counting;
    int destroyed = 0;
    {
        auto array = irama::OwnedArray<Counted>::create(counting.allocator(), 4);
        REQUIRE(array.has_value());
        for (auto& element : *array) {
            element.destroyed = &destroyed;
        }
    }
    REQUIRE(destroyed == 4);
    REQUIRE(counting.live().empty());
}

TEST_CASE("an owned array of no elements allocates nothing", "[base]") {
    irama::test::CountingAllocator counting;
    const auto array = irama::OwnedArray<int>::create(counting.allocator(), 0);

    REQUIRE(array.has_value());
    REQUIRE(array->empty());
    REQUIRE(array->data() == nullptr);
    REQUIRE(counting.allocations() == 0);
}

TEST_CASE("an owned array reports running out of memory", "[base]") {
    irama::test::CountingAllocator counting;
    counting.fail_next();
    const auto array = irama::OwnedArray<int>::create(counting.allocator(), 5);

    REQUIRE(array.error() == irama::AllocationError::out_of_memory);
}

TEST_CASE("an owned array too long for a size_t of bytes runs out of memory without allocating",
          "[base]") {
    irama::test::CountingAllocator counting;
    const auto array = irama::OwnedArray<int>::create(
        counting.allocator(), (std::numeric_limits<std::size_t>::max() / sizeof(int)) + 1);

    REQUIRE(array.error() == irama::AllocationError::out_of_memory);
    REQUIRE(counting.allocations() == 0);
}

TEST_CASE("moving an owned array hands its elements over, to be freed once", "[base]") {
    irama::test::CountingAllocator counting;
    auto first = irama::OwnedArray<int>::create(counting.allocator(), 3);
    auto second = irama::OwnedArray<int>::create(counting.allocator(), 2);
    REQUIRE(first.has_value());
    REQUIRE(second.has_value());
    (*first)[1] = 7;
    const int* elements = first->data();

    irama::OwnedArray<int> moved{std::move(*first)};
    REQUIRE(moved.data() == elements);
    REQUIRE(moved.size() == 3);
    // NOLINTNEXTLINE(bugprone-use-after-move,clang-analyzer-cplusplus.Move): a move empties it.
    REQUIRE(first->empty());

    *second = std::move(moved);
    REQUIRE(counting.frees() == 1);
    REQUIRE(second->data() == elements);
    REQUIRE((*second)[1] == 7);

    *second = irama::OwnedArray<int>{};
    REQUIRE(counting.frees() == 2);
    REQUIRE(counting.mismatched_frees() == 0);
    REQUIRE(counting.live().empty());
}

TEST_CASE("an owned array's elements are read and written inside a render scope", "[base]") {
    irama::test::CountingAllocator counting;
    auto array = irama::OwnedArray<float>::create(counting.allocator(), 4);
    REQUIRE(array.has_value());

    auto& elements = *array;
    render([&] {
        for (std::size_t index = 0; index < elements.size(); ++index) {
            elements[index] = static_cast<float>(index);
        }
        std::ranges::reverse(elements.span());
    });
    REQUIRE(std::ranges::equal(*array, std::array{3.0F, 2.0F, 1.0F, 0.0F}));
}
