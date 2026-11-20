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

#include <irama/engine/base/assertion.hpp>
#include <irama/engine/base/narrow.hpp>
#include <irama/engine/base/nonblocking.hpp>

#include <algorithm>
#include <array>
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
};

int run(std::string_view name) {
    const auto* const found = std::ranges::find(checks, name, &NamedCheck::name);
    if (found == checks.end()) {
        return usage_error;
    }
    found->check(3);
    return EXIT_SUCCESS;
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
