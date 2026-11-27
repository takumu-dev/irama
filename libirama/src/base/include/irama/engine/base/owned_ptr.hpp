// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

/// \file
/// The owning pointer to one object, which frees it through the allocator it came from.

#pragma once

#include <irama/engine/base/allocator.hpp>

#include <expected>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

namespace irama {

/// Destroys an object made by `make_owned()` and frees it through the allocator it came from.
///
/// \tparam T The type of the object.
template <typename T> struct AllocatorDelete {
    /// The allocator the object came from.
    Allocator allocator;

    /// Destroys the object and frees its memory, with the size and alignment of `T`.
    ///
    /// \param object The object, never null.
    void operator()(T* object) const {
        std::destroy_at(object);
        deallocate(allocator, object, sizeof(T), alignof(T));
    }
};

/// The single owner of one object, which it frees through the allocator it came from.
///
/// It is a `std::unique_ptr`, whose deleter holds the allocator, so no default allocator is ever
/// involved. Make one with `make_owned()`.
///
/// \tparam T The type of the object, which isn't an array: `OwnedArray` holds those.
template <typename T>
    requires(!std::is_array_v<T>)
using OwnedPtr = std::unique_ptr<T, AllocatorDelete<T>>;

/// Makes an object in memory from `allocator`, owned by the pointer returned.
///
/// The memory has the size and alignment of `T`, which the pointer frees it with.
///
/// \tparam T The type of the object to make.
/// \tparam Arguments The types of its constructor's arguments.
/// \param allocator The allocator to draw on.
/// \param arguments The arguments to construct the object with.
/// \return The pointer that owns the object, or `AllocationError::out_of_memory` when the allocator
///         has no memory.
template <typename T, typename... Arguments>
    requires(!std::is_array_v<T>) && std::is_constructible_v<T, Arguments...>
[[nodiscard]] std::expected<OwnedPtr<T>, AllocationError> make_owned(const Allocator& allocator,
                                                                     Arguments&&... arguments) {
    void* memory = allocate(allocator, sizeof(T), alignof(T));
    if (memory == nullptr) {
        return std::unexpected(AllocationError::out_of_memory);
    }
    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory): the pointer returned owns it at once.
    T* object = ::new (memory) T(std::forward<Arguments>(arguments)...);
    return OwnedPtr<T>(object, AllocatorDelete<T>{.allocator = allocator});
}

} // namespace irama
