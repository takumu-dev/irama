// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

// Fails the check its argument names, so that a test can watch how the program ends. It is built
// twice, with NDEBUG and without it, whatever the build type, so that every preset checks
// IRAMA_DEBUG_ASSERT both ways.
#ifdef IRAMA_TEST_NDEBUG
#ifndef NDEBUG
#define NDEBUG
#endif
#else
#undef NDEBUG
#endif

#include <irama/engine/base/allocator.hpp>
#include <irama/engine/base/assertion.hpp>
#include <irama/engine/base/narrow.hpp>
#include <irama/engine/base/nonblocking.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <span>
#include <string_view>

namespace {

// The exit status of an argument the program doesn't know.
constexpr int usage_error = 2;

// Each check is handed a triangle, and fails if it can.
using Check = void (*)(int sides);

void fail_assert(int sides) { IRAMA_ASSERT(sides == 4, "a square has four sides"); }

void fail_debug_assert(int sides) { IRAMA_DEBUG_ASSERT(sides == 4, "a square has four sides"); }

// RealtimeSanitizer holds this to the real-time rules, as it does every function so marked.
void fail_nonblocking(int sides) IRAMA_NONBLOCKING {
    IRAMA_ASSERT(sides == 4, "a square has four sides");
}

void narrow_too_large(int sides) { static_cast<void>(irama::narrow<std::uint8_t>(sides * 100)); }

void narrow_negative(int sides) { static_cast<void>(irama::narrow<unsigned>(-sides)); }

// A triangle's sides less three is no bytes at all, and three is no alignment.
void allocate_nothing(int sides) {
    void* block = irama::allocate(irama::default_allocator(), irama::narrow<std::size_t>(sides - 3),
                                  alignof(int));
    irama::deallocate(irama::default_allocator(), block, sizeof(int), alignof(int));
}

void allocate_misaligned(int sides) {
    void* block =
        irama::allocate(irama::default_allocator(), sizeof(int), irama::narrow<std::size_t>(sides));
    irama::deallocate(irama::default_allocator(), block, sizeof(int), alignof(int));
}

void free_nothing(int sides) {
    void* block = irama::allocate(irama::default_allocator(), sizeof(int), alignof(int));
    irama::deallocate(irama::default_allocator(), block, irama::narrow<std::size_t>(sides - 3),
                      alignof(int));
}

void free_misaligned(int sides) {
    void* block = irama::allocate(irama::default_allocator(), sizeof(int), alignof(int));
    irama::deallocate(irama::default_allocator(), block, sizeof(int),
                      irama::narrow<std::size_t>(sides));
}

struct NamedCheck {
    std::string_view name;
    Check check = nullptr;
};

constexpr std::array checks{
    NamedCheck{.name = "assert", .check = fail_assert},
    NamedCheck{.name = "debug-assert", .check = fail_debug_assert},
    NamedCheck{.name = "nonblocking", .check = fail_nonblocking},
    NamedCheck{.name = "narrow-too-large", .check = narrow_too_large},
    NamedCheck{.name = "narrow-negative", .check = narrow_negative},
    NamedCheck{.name = "allocate-nothing", .check = allocate_nothing},
    NamedCheck{.name = "allocate-misaligned", .check = allocate_misaligned},
    NamedCheck{.name = "free-nothing", .check = free_nothing},
    NamedCheck{.name = "free-misaligned", .check = free_misaligned},
};

int run(std::string_view name) {
    for (const auto& named : checks) {
        if (named.name == name) {
            named.check(3);
            return EXIT_SUCCESS;
        }
    }
    return usage_error;
}

} // namespace

int main(int argc, char** argv) {
#ifdef _MSC_VER
    // MSVC's runtime answers abort() with a dialog in debug builds, and with a Windows Error
    // Reporting fault report, either of which can wait for a click that never comes in CI.
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    const std::span arguments{argv, irama::narrow<std::size_t>(argc)};
    return arguments.size() == 2 ? run(arguments.back()) : usage_error;
}
