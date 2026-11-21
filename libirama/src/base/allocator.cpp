// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include <irama/engine/base/allocator.hpp>

#include "realtime_guard.hpp"
#include <irama/engine/base/assertion.hpp>

#include <bit>
#include <cstddef>
#include <cstdlib>
#include <limits>

#ifdef _WIN32
#include <malloc.h>
#endif

namespace {

void* allocate_from_runtime(void* /*user*/, std::size_t size, std::size_t alignment) {
    // aligned_alloc takes a whole number of alignments, which mustn't wrap round. No block could
    // hold such a size on Windows either.
    if (size > std::numeric_limits<std::size_t>::max() - (alignment - 1)) {
        return nullptr;
    }
#ifdef _WIN32
    // The Windows C runtime has no aligned_alloc.
    return _aligned_malloc(size, alignment);
#else
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory): the heap.
    return std::aligned_alloc(alignment, (size + alignment - 1) / alignment * alignment);
#endif
}

void free_to_runtime(void* /*user*/, void* pointer, std::size_t /*size*/,
                     std::size_t /*alignment*/) {
#ifdef _WIN32
    _aligned_free(pointer);
#else
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory): the heap.
    std::free(pointer);
#endif
}

} // namespace

namespace irama {

Allocator default_allocator() {
    return Allocator{.user = nullptr, .allocate = allocate_from_runtime, .free = free_to_runtime};
}

void* allocate(const Allocator& allocator, std::size_t size, std::size_t alignment) {
    IRAMA_ASSERT(size != 0, "an allocation has at least one byte");
    IRAMA_ASSERT(std::has_single_bit(alignment), "an alignment is a power of two");
    IRAMA_ASSERT(allocator.allocate != nullptr, "an allocator can allocate");
#ifdef IRAMA_REALTIME_GUARD
    detail::guard_allocation(size);
#endif
    return allocator.allocate(allocator.user, size, alignment);
}

void deallocate(const Allocator& allocator, void* pointer, std::size_t size,
                std::size_t alignment) {
    IRAMA_ASSERT(pointer != nullptr, "a block freed is one that was allocated");
    IRAMA_ASSERT(size != 0, "an allocation has at least one byte");
    IRAMA_ASSERT(std::has_single_bit(alignment), "an alignment is a power of two");
    IRAMA_ASSERT(allocator.free != nullptr, "an allocator can free");
#ifdef IRAMA_REALTIME_GUARD
    detail::guard_free(size);
#endif
    allocator.free(allocator.user, pointer, size, alignment);
}

} // namespace irama
