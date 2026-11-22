// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include "audio/device_choice.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <string>

using irama_studio::DeviceChoice;

namespace {

// The devices there are.
[[nodiscard]] std::array<std::string, 2> listed() { return {"USB Audio", "Built-in Audio"}; }

} // namespace

TEST_CASE("an empty device setting opens the system default with no note", "[studio][audio]") {
    CHECK(irama_studio::choose_device("", listed()) == DeviceChoice{});
    CHECK(irama_studio::choose_device("", {}) == DeviceChoice{});
}

TEST_CASE("a listed device opens by its name", "[studio][audio]") {
    CHECK(irama_studio::choose_device("Built-in Audio", listed()) ==
          DeviceChoice{.name = "Built-in Audio"});
}

TEST_CASE("a named device that isn't listed opens the system default, and says so",
          "[studio][audio]") {
    CHECK(irama_studio::choose_device("USB Headset", listed()) ==
          DeviceChoice{.note = "USB Headset not found, using system default"});
    CHECK(irama_studio::choose_device("USB Audio", {}) ==
          DeviceChoice{.note = "USB Audio not found, using system default"});
    // A name must match in full.
    CHECK(irama_studio::choose_device("USB", listed()).name.empty());
    CHECK(irama_studio::choose_device("usb audio", listed()).name.empty());
}

TEST_CASE("a lost named device falls back to the system default, and says so", "[studio][audio]") {
    CHECK(irama_studio::fall_back_after_loss("USB Audio") ==
          DeviceChoice{.note = "USB Audio lost, using system default"});
}

TEST_CASE("a lost system default has nothing to fall back to", "[studio][audio]") {
    CHECK_FALSE(irama_studio::fall_back_after_loss("").has_value());
}

TEST_CASE("the buffer sizes are the four Studio offers, with 256 frames the default",
          "[studio][audio]") {
    CHECK(irama_studio::buffer_sizes == std::array<std::uint32_t, 4>{128, 256, 512, 1024});
    CHECK(irama_studio::default_buffer_size == 256);
}
