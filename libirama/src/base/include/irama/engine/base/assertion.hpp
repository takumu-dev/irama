// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

/// \file
/// Checks of preconditions and invariants, which end the program when they fail.
///
/// A broken precondition is a bug in the caller, not an error to return. `IRAMA_ASSERT` checks
/// cheap preconditions in every build, release included; `IRAMA_DEBUG_ASSERT` checks internal
/// invariants and expensive conditions where `NDEBUG` isn't defined. A failed check writes one line
/// to stderr, naming the condition, why it must hold and where it is written, and aborts. It does
/// so on any thread, the audio thread included, since the program is ending.

#pragma once

#include <source_location>

namespace irama::detail {

/// Reports a failed check on stderr and aborts.
///
/// It turns RealtimeSanitizer off before it reports, so the report isn't taken for a real-time
/// violation, and uses `<cstdio>` alone, allocating nothing.
///
/// \param expression The condition that was false, as it is written in the check.
/// \param message Why the condition must hold.
/// \param location Where the check is written.
[[noreturn]] void fail_assertion(const char* expression, const char* message,
                                 std::source_location location);

} // namespace irama::detail

// NOLINTBEGIN(cppcoreguidelines-macro-usage): a check reports its condition as written and the
// place it is written, which only a macro can see.

/// Checks a cheap precondition, in every build.
///
/// The condition is evaluated once. If it is false, the check writes its report to stderr and
/// aborts.
///
/// \param condition The condition that must hold.
/// \param message Why it must hold, as a string literal.
#define IRAMA_ASSERT(condition, message)                                                           \
    (static_cast<bool>(condition) ? static_cast<void>(0)                                           \
                                  : ::irama::detail::fail_assertion(                               \
                                        #condition, (message), ::std::source_location::current()))

/// Checks an internal invariant or an expensive condition, where `NDEBUG` isn't defined.
///
/// Where `NDEBUG` is defined, as in release builds, the condition is never evaluated, though it
/// must still compile. Elsewhere this is `IRAMA_ASSERT`.
///
/// \param condition The condition that must hold.
/// \param message Why it must hold, as a string literal.
#ifdef NDEBUG
#define IRAMA_DEBUG_ASSERT(condition, message)                                                     \
    static_cast<void>(sizeof(static_cast<bool>(condition)))
#else
#define IRAMA_DEBUG_ASSERT(condition, message) IRAMA_ASSERT(condition, message)
#endif

// NOLINTEND(cppcoreguidelines-macro-usage)
