// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include "app/options.hpp"

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <string_view>
#include <vector>

namespace {

constexpr std::string_view bad_frame_count =
    "irama-studio: --exit-after-frames takes a whole number of frames above 0";

} // namespace

TEST_CASE("Studio runs until the user quits when its command line asks for nothing", "[studio]") {
    const auto options = irama_studio::parse_options({});
    REQUIRE(options.has_value());
    CHECK_FALSE(options->exit_after_frames.has_value());
}

TEST_CASE("Studio quits after drawing the frames that --exit-after-frames gives", "[studio]") {
    constexpr std::array<std::string_view, 2> arguments{"--exit-after-frames", "300"};
    const auto options = irama_studio::parse_options(arguments);
    REQUIRE(options.has_value());
    CHECK(options->exit_after_frames == 300U);
}

TEST_CASE("Studio refuses a frame count that isn't a whole number above 0", "[studio]") {
    for (const std::string_view count : {"0", "-1", "2.5", "300frames", "many", ""}) {
        CAPTURE(count);
        const std::array<std::string_view, 2> arguments{"--exit-after-frames", count};
        const auto options = irama_studio::parse_options(arguments);
        REQUIRE_FALSE(options.has_value());
        CHECK(options.error() == bad_frame_count);
    }
}

TEST_CASE("Studio refuses --exit-after-frames without a frame count", "[studio]") {
    constexpr std::array<std::string_view, 1> arguments{"--exit-after-frames"};
    const auto options = irama_studio::parse_options(arguments);
    REQUIRE_FALSE(options.has_value());
    CHECK(options.error() == bad_frame_count);
}

TEST_CASE("an argument Studio doesn't know prints the usage", "[studio]") {
    const std::vector<std::vector<std::string_view>> command_lines{
        {"--fullscreen"},
        {"song.irama"},
        {"--exit-after-frames", "10", "--fullscreen"},
    };
    for (const auto& arguments : command_lines) {
        CAPTURE(arguments);
        const auto options = irama_studio::parse_options(arguments);
        REQUIRE_FALSE(options.has_value());
        CHECK(options.error() == irama_studio::usage);
    }
}
