// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

/// \file
/// The owning array whose length is fixed when it is made, which frees its elements through the
/// allocator they came from.

#pragma once

#include <irama/engine/base/allocator.hpp>
#include <irama/engine/base/assertion.hpp>

#include <concepts>
#include <cstddef>
#include <expected>
#include <limits>
#include <memory>
#include <span>
#include <utility>

namespace irama {

/// An array of `T` whose length is set when it is made and never changes, owning its elements and
/// freeing them through the allocator they came from.
///
/// It is libirama's container for anything sized at run time, in place of a standard container,
/// which could grow, and would draw on the default allocator. Make one with `create()`. It moves,
/// handing its elements over, but doesn't copy. Moving one leaves it empty.
///
/// \tparam T The type of the elements.
template <typename T> class OwnedArray {
  public:
    /// Makes an array of `length` value-initialised elements, in memory from `allocator`.
    ///
    /// The memory has the alignment of `T` and room for `length` elements, which the array frees it
    /// with. An array of no elements allocates nothing.
    ///
    /// \param allocator The allocator to draw on.
    /// \param length How many elements the array holds.
    /// \return The array, or `AllocationError::out_of_memory` when the allocator has no memory or
    ///         `length` elements can't be represented in a `std::size_t` of bytes.
    [[nodiscard]] static std::expected<OwnedArray, AllocationError>
    create(const Allocator& allocator, std::size_t length)
        requires std::default_initializable<T>
    {
        T* elements = allocate_elements(allocator, length);
        if (elements == nullptr && length != 0) {
            return std::unexpected(AllocationError::out_of_memory);
        }
        std::uninitialized_value_construct_n(elements, length);
        return OwnedArray{allocator, elements, length};
    }

    /// Makes an array of `length` copies of `value`, in memory from `allocator`, as the other
    /// `create()` does, for elements that can't be made with no arguments.
    ///
    /// \param allocator The allocator to draw on.
    /// \param length How many elements the array holds.
    /// \param value What each element is a copy of.
    /// \return The array, or `AllocationError::out_of_memory` when the allocator has no memory or
    ///         `length` elements can't be represented in a `std::size_t` of bytes.
    [[nodiscard]] static std::expected<OwnedArray, AllocationError>
    create(const Allocator& allocator, std::size_t length, const T& value)
        requires std::copy_constructible<T>
    {
        T* elements = allocate_elements(allocator, length);
        if (elements == nullptr && length != 0) {
            return std::unexpected(AllocationError::out_of_memory);
        }
        std::uninitialized_fill_n(elements, length, value);
        return OwnedArray{allocator, elements, length};
    }

    /// Makes an empty array, which owns nothing.
    OwnedArray() = default;

    /// Takes the elements of `other`, leaving it empty.
    ///
    /// \param other The array to take from.
    OwnedArray(OwnedArray&& other) noexcept
        : allocator_{other.allocator_}, elements_{std::exchange(other.elements_, nullptr)},
          length_{std::exchange(other.length_, 0)} {}

    /// Frees this array's elements, then takes those of `other`, leaving it empty.
    ///
    /// \param other The array to take from.
    /// \return This array.
    OwnedArray& operator=(OwnedArray&& other) noexcept {
        if (this != &other) {
            release();
            allocator_ = other.allocator_;
            elements_ = std::exchange(other.elements_, nullptr);
            length_ = std::exchange(other.length_, 0);
        }
        return *this;
    }

    OwnedArray(const OwnedArray&) = delete;
    OwnedArray& operator=(const OwnedArray&) = delete;

    /// Destroys the elements and frees their memory.
    ~OwnedArray() { release(); }

    /// \return How many elements the array holds.
    [[nodiscard]] std::size_t size() const { return length_; }

    /// \return Whether the array holds no elements.
    [[nodiscard]] bool empty() const { return length_ == 0; }

    /// \return The first element, or null for an empty array.
    [[nodiscard]] T* data() { return elements_; }

    /// \return The first element, or null for an empty array.
    [[nodiscard]] const T* data() const { return elements_; }

    /// \return The elements, as a span.
    [[nodiscard]] std::span<T> span() { return {elements_, length_}; }

    /// \return The elements, as a span.
    [[nodiscard]] std::span<const T> span() const { return {elements_, length_}; }

    /// \return The first element, or the end of an empty array.
    [[nodiscard]] T* begin() { return elements_; }

    /// \return The first element, or the end of an empty array.
    [[nodiscard]] const T* begin() const { return elements_; }

    /// \return The place after the last element.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic): the end of the elements.
    [[nodiscard]] T* end() { return elements_ + length_; }

    /// \return The place after the last element.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic): as above.
    [[nodiscard]] const T* end() const { return elements_ + length_; }

    /// \param index Which element, less than `size()`, which is checked where `NDEBUG` isn't
    ///              defined.
    /// \return The element at `index`.
    [[nodiscard]] T& operator[](std::size_t index) {
        IRAMA_DEBUG_ASSERT(index < length_, "an element is read within the array");
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic): checked above.
        return elements_[index];
    }

    /// \param index Which element, less than `size()`, which is checked where `NDEBUG` isn't
    ///              defined.
    /// \return The element at `index`.
    [[nodiscard]] const T& operator[](std::size_t index) const {
        IRAMA_DEBUG_ASSERT(index < length_, "an element is read within the array");
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic): checked above.
        return elements_[index];
    }

  private:
    // The memory for `length` elements, or null for none or when the allocator has no memory.
    [[nodiscard]] static T* allocate_elements(const Allocator& allocator, std::size_t length) {
        if (length == 0 || length > std::numeric_limits<std::size_t>::max() / sizeof(T)) {
            return nullptr;
        }
        return static_cast<T*>(allocate(allocator, length * sizeof(T), alignof(T)));
    }

    OwnedArray(const Allocator& allocator, T* elements, std::size_t length)
        : allocator_{allocator}, elements_{elements}, length_{length} {}

    // Destroys the elements and frees their memory, with the size and alignment create() gave it.
    void release() {
        if (elements_ == nullptr) {
            return;
        }
        std::destroy_n(elements_, length_);
        deallocate(allocator_, elements_, length_ * sizeof(T), alignof(T));
        elements_ = nullptr;
        length_ = 0;
    }

    Allocator allocator_;
    T* elements_ = nullptr;
    std::size_t length_ = 0;
};

} // namespace irama
