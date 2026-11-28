// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

/// \file
/// An instrument part: one plain value for each of an instrument's parameters.

#pragma once

#include <irama/engine/base/allocator.hpp>
#include <irama/engine/base/owned_array.hpp>
#include <irama/engine/base/owned_ptr.hpp>

#include <expected>
#include <span>

namespace irama {

class InstrumentPart;

/// The single owner of an instrument part.
using InstrumentPartPtr = OwnedPtr<InstrumentPart>;

/// The part of an EngineSong that holds one instrument: a plain value for each of its parameters,
/// in the order of the instrument's parameter table, which the voices give meaning to.
///
/// An instrument part never changes once it is built.
class InstrumentPart {
    // Lets create() alone construct a part, through make_owned().
    struct Key {
        explicit Key() = default;
    };

  public:
    /// Builds an instrument part, on a control thread.
    ///
    /// \param allocator The allocator to draw on, which the part is freed to.
    /// \param values The plain values, at least one, none of them NaN, which is checked. They are
    ///               copied.
    /// \return The part, or `AllocationError::out_of_memory` when the allocator has no memory.
    [[nodiscard]] static std::expected<InstrumentPartPtr, AllocationError>
    create(const Allocator& allocator, std::span<const float> values);

    /// Makes the part, for `create()` alone.
    ///
    /// \param key Which only `create()` can make.
    /// \param values The values' storage, already filled.
    InstrumentPart(Key key, OwnedArray<float> values);

    /// \return The plain values, one for each parameter.
    [[nodiscard]] std::span<const float> values() const { return values_.span(); }

    /// \return Whether two parts hold the same values.
    friend bool operator==(const InstrumentPart& left, const InstrumentPart& right);

  private:
    OwnedArray<float> values_;
};

} // namespace irama
