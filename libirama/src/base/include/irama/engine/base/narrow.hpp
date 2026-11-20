// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

/// \file
/// The checked narrowing of one integer type to another.

#pragma once

#include <irama/engine/base/assertion.hpp>

#include <concepts>
#include <source_location>
#include <utility>

namespace irama {

/// Converts an integer to another integer type, which must hold its value exactly.
///
/// Counts and indices are `std::size_t`, and every narrowing integer conversion goes through this,
/// so a value that doesn't fit is caught where it is converted rather than wrapping silently. The
/// precondition is checked in every build, as an `IRAMA_ASSERT` would, and its report names the
/// call. In a constant expression a value that doesn't fit fails to compile. `std::in_range`
/// refuses `bool` and the character types, so they don't compile either.
///
/// \tparam To The integer type to convert to.
/// \tparam From The integer type to convert from.
/// \param value The integer to convert, which `To` must be able to hold.
/// \param location Where the conversion is written, which the caller leaves to its default.
/// \return The same value as a `To`.
template <std::integral To, std::integral From>
[[nodiscard]] constexpr To narrow(From value,
                                  std::source_location location = std::source_location::current()) {
    if (!std::in_range<To>(value)) {
        detail::fail_assertion("std::in_range<To>(value)",
                               "the value fits the type it is narrowed to", location);
    }
    return static_cast<To>(value);
}

} // namespace irama
