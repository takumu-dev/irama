// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

/// \file
/// Where libirama's memory comes from: the allocator each object is given, and the one entry point
/// through which libirama allocates and frees.

#pragma once

#include <cstddef>
#include <cstdint>

namespace irama {

/// A source of memory, given to each object that allocates when it is created, and kept by it to
/// free what it allocated.
///
/// It is a pair of callbacks with a pointer of the host's own, so a host can hand libirama an arena
/// or a pool, and two objects can draw on different ones. `free` is given back the size and
/// alignment that the block was allocated with, so an allocator needs no header of its own. There
/// is no reallocation, since libirama never grows what it has allocated.
///
/// libirama calls neither callback directly, only through `allocate()` and `deallocate()`.
struct Allocator {
    /// The host's own pointer, handed to both callbacks as it is.
    void* user = nullptr;
    /// Returns a block of at least `size` bytes, aligned to `alignment`, or null when there is no
    /// memory. `size` is never zero, and `alignment` is a power of two.
    void* (*allocate)(void* user, std::size_t size, std::size_t alignment) = nullptr;
    /// Frees a block that `allocate` returned, given the same `size` and `alignment`.
    void (*free)(void* user, void* pointer, std::size_t size, std::size_t alignment) = nullptr;
};

/// The error of an allocation that found no memory.
enum class AllocationError : std::uint8_t {
    /// The allocator returned null, or the size asked for can't be represented.
    out_of_memory,
};

/// Returns the allocator that draws on the C runtime's aligned allocation.
///
/// It uses `std::aligned_alloc` and `std::free`, or `_aligned_malloc` and `_aligned_free` on
/// Windows, whose C runtime has no `aligned_alloc`. It rounds each size up to a whole number of
/// alignments, as `aligned_alloc` requires, and returns null for a size that can't be rounded.
///
/// \return The default allocator, which keeps no state.
[[nodiscard]] Allocator default_allocator();

/// Allocates through `allocator`, as all of libirama's allocations do.
///
/// Inside a render call this breaks the real-time rules, so where the real-time guard is compiled
/// in, it stops the program with its report, as it does for `operator new`. Elsewhere it calls the
/// allocator.
///
/// \param allocator The allocator to draw on, both of whose callbacks are set.
/// \param size How many bytes to allocate, at least one.
/// \param alignment The alignment of the block, a power of two.
/// \return The block, or null when the allocator has no memory.
[[nodiscard]] void* allocate(const Allocator& allocator, std::size_t size, std::size_t alignment);

/// Frees through `allocator`, as all of libirama's frees do.
///
/// Inside a render call this breaks the real-time rules, so where the real-time guard is compiled
/// in, it stops the program with its report, as it does for `operator delete`. Elsewhere it calls
/// the allocator.
///
/// \param allocator The allocator that `pointer` was allocated from.
/// \param pointer The block, as `allocate()` returned it, never null.
/// \param size The size it was allocated with.
/// \param alignment The alignment it was allocated with.
void deallocate(const Allocator& allocator, void* pointer, std::size_t size, std::size_t alignment);

} // namespace irama
