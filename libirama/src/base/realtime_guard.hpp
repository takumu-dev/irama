// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

// The real-time guard's checks, for libirama's allocation entry point, in a build that compiles the
// guard in. They live in render_scope.cpp, beside the count of open scopes they read.

#pragma once

// Most builds leave the guard out, so include checks would take the include for an unused one.
// IWYU pragma: always_keep

#ifdef IRAMA_REALTIME_GUARD

#include <cstddef>

namespace irama::detail {

// Stops the program with the guard's report if a render scope is open on this thread, saying that
// `size` bytes were allocated.
void guard_allocation(std::size_t size);

// Stops the program with the guard's report if a render scope is open on this thread, saying that
// `size` bytes were freed.
void guard_free(std::size_t size);

} // namespace irama::detail

#endif
