// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

/// \file
/// The mark of a function that must keep the real-time rules.

#pragma once

// NOLINTBEGIN(cppcoreguidelines-macro-usage): the attribute exists on Clang alone, and GCC warns
// about one it doesn't know, so only the preprocessor can choose.

/// Marks a function that must keep the real-time rules: no allocation, no lock, no I/O, no wait.
///
/// It goes after the parameter list, as in `void render() IRAMA_NONBLOCKING;`. On Clang it is
/// `[[clang::nonblocking]]`, which RealtimeSanitizer checks at run time, stopping the program at
/// the first call that breaks the rules. Elsewhere it is nothing.
#ifdef __clang__
#define IRAMA_NONBLOCKING [[clang::nonblocking]]
#else
#define IRAMA_NONBLOCKING
#endif

// NOLINTEND(cppcoreguidelines-macro-usage)
