// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include <irama/engine/base/render_scope.hpp>

#include "realtime_guard.hpp"

#if defined(__x86_64__) || defined(_M_X64)
#define IRAMA_MXCSR
#include <xmmintrin.h>
#endif

#ifdef IRAMA_REALTIME_GUARD
#include "realtime_sanitizer.hpp"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <optional>

#ifdef _WIN32
#include <malloc.h>
#else
#include <limits>
#endif

namespace {

// How many render scopes are open on this thread.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): the guard's state.
thread_local std::size_t open_scopes = 0;

} // namespace
#endif

#ifdef IRAMA_MXCSR
namespace {

// The bits of MXCSR that flush a denormal result to zero, and read a denormal operand as zero.
constexpr unsigned int flush_to_zero = 1U << 15U;
constexpr unsigned int denormals_are_zero = 1U << 6U;

} // namespace
#endif

namespace irama {

// These stay out of the header, so that a program that renders links this file, and with it the
// real-time guard's operators below: a linker takes an object from a static library only for a
// symbol still undefined, and the C++ runtime, or a sanitizer's, already defines operator new.
// Out of line, they are opaque calls too, so no compiler moves floating-point work across the
// writes to MXCSR.
RenderScope::RenderScope()
#ifdef IRAMA_MXCSR
    : saved_control_{_mm_getcsr()}
#endif
{
#ifdef IRAMA_REALTIME_GUARD
    ++open_scopes;
#endif
#ifdef IRAMA_MXCSR
    _mm_setcsr(saved_control_ | flush_to_zero | denormals_are_zero);
#endif
}

RenderScope::~RenderScope() {
#ifdef IRAMA_MXCSR
    _mm_setcsr(saved_control_);
#endif
#ifdef IRAMA_REALTIME_GUARD
    --open_scopes;
#endif
}

} // namespace irama

#ifdef IRAMA_REALTIME_GUARD

namespace {

// Reports memory allocated or freed inside a render call, of `size` bytes where that is known, and
// aborts.
[[noreturn]] void stop(const char* action, std::optional<std::size_t> size) {
    // The report must not trip the guard again, however the C library writes it.
    open_scopes = 0;
#ifdef IRAMA_REALTIME_SANITIZER
    // Nor may RealtimeSanitizer take the report for a real-time violation of its own.
    const __rtsan::ScopedDisabler real_time_checks_off;
#endif
    // NOLINTBEGIN(cppcoreguidelines-pro-type-vararg,modernize-use-std-print): it may allocate.
    if (size) {
        std::fprintf(stderr, "real-time guard: %s %zu bytes inside a render call\n", action, *size);
    } else {
        std::fprintf(stderr, "real-time guard: %s memory inside a render call\n", action);
    }
    // NOLINTEND(cppcoreguidelines-pro-type-vararg,modernize-use-std-print)
    // abort() flushes no stream, and a host may have given stderr a buffer.
    std::fflush(stderr);
    std::abort();
}

// Allocates `size` bytes, with `alignment` if one is given. Returns null if it can't.
void* allocate(std::size_t size, std::optional<std::align_val_t> alignment = std::nullopt) {
    if (open_scopes != 0) {
        stop("allocated", size);
    }
    // operator new returns a distinct pointer even for zero bytes.
    size = size == 0 ? 1 : size;
    if (!alignment) {
        // NOLINTNEXTLINE(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory): the heap.
        return std::malloc(size);
    }
    const auto bytes = static_cast<std::size_t>(*alignment);
#ifdef _WIN32
    // The Windows C runtime has no aligned_alloc.
    return _aligned_malloc(size, bytes);
#else
    // aligned_alloc takes a whole number of alignments, which mustn't wrap round.
    if (size > std::numeric_limits<std::size_t>::max() - bytes) {
        return nullptr;
    }
    return std::aligned_alloc(bytes, (size + bytes - 1) / bytes * bytes);
#endif
}

// Allocates as allocate() does, for the operators that may not return null. libirama has no
// exceptions, so they can't throw std::bad_alloc, and end the program instead.
void* allocate_or_abort(std::size_t size,
                        std::optional<std::align_val_t> alignment = std::nullopt) {
    void* memory = allocate(size, alignment);
    if (memory == nullptr) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,modernize-use-std-print): see stop().
        std::fprintf(stderr, "out of memory: %zu bytes could not be allocated\n", size);
        std::fflush(stderr);
        std::abort();
    }
    return memory;
}

// Frees memory from allocate(), given the `size` it was allocated with where the caller knows it,
// and the `alignment` it was allocated with, if any.
void release(void* memory, std::optional<std::size_t> size,
             std::optional<std::align_val_t> alignment = std::nullopt) {
    if (memory == nullptr) {
        return;
    }
    if (open_scopes != 0) {
        stop("freed", size);
    }
#ifdef _WIN32
    if (alignment) {
        _aligned_free(memory);
        return;
    }
#else
    static_cast<void>(alignment);
#endif
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory): the heap.
    std::free(memory);
}

} // namespace

namespace irama::detail {

void guard_allocation(std::size_t size) {
    if (open_scopes != 0) {
        stop("allocated", size);
    }
}

void guard_free(std::size_t size) {
    if (open_scopes != 0) {
        stop("freed", size);
    }
}

} // namespace irama::detail

#ifdef _MSC_VER
// MSVC's <new> annotates these operators for its code analysis, and warns about a definition that
// doesn't repeat the annotations.
#pragma warning(disable : 28251)
#endif

// The 20 replaceable allocation and deallocation functions. Each repeats the noexcept of its
// declaration in <new>, which it must match.

// The 8 allocation functions.

void* operator new(std::size_t size) { return allocate_or_abort(size); }

void* operator new[](std::size_t size) { return allocate_or_abort(size); }

void* operator new(std::size_t size, std::align_val_t alignment) {
    return allocate_or_abort(size, alignment);
}

void* operator new[](std::size_t size, std::align_val_t alignment) {
    return allocate_or_abort(size, alignment);
}

void* operator new(std::size_t size, const std::nothrow_t& /*unused*/) noexcept {
    return allocate(size);
}

void* operator new[](std::size_t size, const std::nothrow_t& /*unused*/) noexcept {
    return allocate(size);
}

void* operator new(std::size_t size, std::align_val_t alignment,
                   const std::nothrow_t& /*unused*/) noexcept {
    return allocate(size, alignment);
}

void* operator new[](std::size_t size, std::align_val_t alignment,
                     const std::nothrow_t& /*unused*/) noexcept {
    return allocate(size, alignment);
}

// The 12 deallocation functions.

void operator delete(void* memory) noexcept { release(memory, std::nullopt); }

void operator delete[](void* memory) noexcept { release(memory, std::nullopt); }

void operator delete(void* memory, std::size_t size) noexcept { release(memory, size); }

void operator delete[](void* memory, std::size_t size) noexcept { release(memory, size); }

void operator delete(void* memory, const std::nothrow_t& /*unused*/) noexcept {
    release(memory, std::nullopt);
}

void operator delete[](void* memory, const std::nothrow_t& /*unused*/) noexcept {
    release(memory, std::nullopt);
}

void operator delete(void* memory, std::align_val_t alignment) noexcept {
    release(memory, std::nullopt, alignment);
}

void operator delete[](void* memory, std::align_val_t alignment) noexcept {
    release(memory, std::nullopt, alignment);
}

void operator delete(void* memory, std::size_t size, std::align_val_t alignment) noexcept {
    release(memory, size, alignment);
}

void operator delete[](void* memory, std::size_t size, std::align_val_t alignment) noexcept {
    release(memory, size, alignment);
}

void operator delete(void* memory, std::align_val_t alignment,
                     const std::nothrow_t& /*unused*/) noexcept {
    release(memory, std::nullopt, alignment);
}

void operator delete[](void* memory, std::align_val_t alignment,
                       const std::nothrow_t& /*unused*/) noexcept {
    release(memory, std::nullopt, alignment);
}

#endif
