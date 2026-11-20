// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

/// \file
/// The span of a render call, which the real-time guard watches.

#pragma once

namespace irama {

/// Marks the length of a render call on the thread that makes it.
///
/// A render entry point holds one for as long as it runs. Where the real-time guard is compiled in,
/// as `IRAMA_REALTIME_GUARD` does, code that allocates or frees memory through `operator new` or
/// `operator delete` while a scope is open on its thread stops the program: the guard writes one
/// line to stderr, saying what was allocated or freed, and aborts. Elsewhere a scope does nothing.
///
/// Scopes nest, and the guard watches until the outermost one ends. Each thread has its own, so a
/// thread that isn't rendering may allocate while another renders.
class RenderScope {
  public:
    /// Opens a scope on this thread.
    [[nodiscard]] RenderScope();

    /// Closes the scope.
    ~RenderScope();

    RenderScope(const RenderScope&) = delete;
    RenderScope& operator=(const RenderScope&) = delete;
    RenderScope(RenderScope&&) = delete;
    RenderScope& operator=(RenderScope&&) = delete;
};

} // namespace irama
