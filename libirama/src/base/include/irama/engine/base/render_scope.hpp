// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

/// \file
/// The span of a render call, which the real-time guard watches, and which renders in one
/// floating-point environment.

#pragma once

namespace irama {

/// Marks the length of a render call on the thread that makes it.
///
/// A render entry point holds one for as long as it runs. Where the real-time guard is compiled in,
/// as `IRAMA_REALTIME_GUARD` does, code that allocates or frees memory through `operator new`,
/// `operator delete`, or libirama's allocation entry point, while a scope is open on its thread
/// stops the program: the guard writes one line to stderr, saying what was allocated or freed, and
/// aborts.
///
/// In every build, on x86-64, opening a scope also saves the thread's MXCSR and sets its
/// flush-to-zero and denormals-are-zero bits, and closing it puts the saved value back. So code
/// inside a render call always sees denormals as zero, whatever the host has set, and the host gets
/// its own environment back afterwards. On other targets the floating-point environment is left as
/// it is.
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

  private:
    // The thread's MXCSR when the scope opened. Targets other than x86-64 have none to keep.
    [[maybe_unused]] unsigned int saved_control_ = 0;
};

} // namespace irama
