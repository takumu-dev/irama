// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

// SDL drops a video driver whose development packages were missing when it was built, and the
// build still passes. These tests turn a dropped driver into a failure.

#include <irama/engine/base/narrow.hpp>

#include <SDL3/SDL_video.h>
#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <string_view>
#include <vector>

namespace {

// The video drivers compiled into SDL, which SDL lists without opening a window.
[[nodiscard]] std::vector<std::string_view> compiled_video_drivers() {
    const int count = SDL_GetNumVideoDrivers();
    std::vector<std::string_view> drivers;
    drivers.reserve(irama::narrow<std::size_t>(count));
    for (int index = 0; index < count; ++index) {
        drivers.emplace_back(SDL_GetVideoDriver(index));
    }
    return drivers;
}

} // namespace

#ifdef _WIN32
TEST_CASE("SDL's video drivers include windows", "[studio]") {
    const std::vector<std::string_view> drivers = compiled_video_drivers();
    CAPTURE(drivers);
    CHECK(std::ranges::contains(drivers, std::string_view{"windows"}));
}
#else
TEST_CASE("SDL's video drivers include wayland, x11 and offscreen", "[studio]") {
    const std::vector<std::string_view> drivers = compiled_video_drivers();
    CAPTURE(drivers);
    for (const std::string_view driver : {"wayland", "x11", "offscreen"}) {
        CAPTURE(driver);
        CHECK(std::ranges::contains(drivers, driver));
    }
}
#endif
