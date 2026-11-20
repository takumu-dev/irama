// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

// Allocates or frees memory inside or outside a render scope, as its argument names, so that a test
// can watch how the program ends: stopped by the real-time guard or RealtimeSanitizer, or exiting 0
// where neither is compiled in.

#include <irama/engine/base/render_scope.hpp>

#include <irama/engine/base/narrow.hpp>
#include <irama/engine/base/nonblocking.hpp>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <memory>
#include <span>
#include <string_view>
#include <thread>

namespace {

// The exit status of an argument the program doesn't know.
constexpr int usage_error = 2;

// Where each case keeps what it allocates. It outlives the case, so the compiler can't leave out an
// allocation that nothing reads.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): see above.
std::unique_ptr<int> held;

// A type aligned beyond what operator new gives by default, so that it takes the aligned forms. It
// fills its alignment, since MSVC warns about a type padded out to one.
struct alignas(64) Aligned {
    std::array<std::byte, 64> bytes{};
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): as `held`.
std::unique_ptr<Aligned> held_aligned;

void allocate() { held = std::make_unique<int>(3); }

void free_memory() { held.reset(); }

void allocate_aligned() { held_aligned = std::make_unique<Aligned>(); }

void free_aligned() { held_aligned.reset(); }

// Stands in for a render entry point: it opens a render scope for the length of the call, and
// RealtimeSanitizer holds it to the real-time rules.
template <typename Body> void render(Body body) IRAMA_NONBLOCKING {
    const irama::RenderScope scope;
    body();
}

void new_inside() { render(allocate); }

void delete_inside() {
    allocate();
    render(free_memory);
}

void aligned_new_inside() { render(allocate_aligned); }

void aligned_delete_inside() {
    allocate_aligned();
    render(free_aligned);
}

void new_outside() {
    allocate();
    free_memory();
    allocate_aligned();
    free_aligned();
}

// A scope that ends leaves the one around it open.
void new_after_nested() {
    render([] {
        render([] {});
        allocate();
    });
}

// Another thread renders while this one allocates, which is allowed: only the thread making a
// render call is held to the real-time rules.
void new_beside() {
    std::atomic<bool> rendering = false;
    std::atomic<bool> allocated = false;
    std::thread renderer{[&] {
        render([&] {
            rendering = true;
            while (!allocated) {
            }
        });
    }};
    while (!rendering) {
    }
    allocate();
    allocated = true;
    renderer.join();
    free_memory();
}

using Case = void (*)();

struct NamedCase {
    std::string_view name;
    Case run = nullptr;
};

constexpr std::array cases{
    NamedCase{.name = "new-inside", .run = new_inside},
    NamedCase{.name = "delete-inside", .run = delete_inside},
    NamedCase{.name = "aligned-new-inside", .run = aligned_new_inside},
    NamedCase{.name = "aligned-delete-inside", .run = aligned_delete_inside},
    NamedCase{.name = "new-outside", .run = new_outside},
    NamedCase{.name = "new-after-nested", .run = new_after_nested},
    NamedCase{.name = "new-beside", .run = new_beside},
};

int run(std::string_view name) {
    for (const auto& named : cases) {
        if (named.name == name) {
            named.run();
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
