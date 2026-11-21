// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include "app/settings.hpp"

#include "app/config_files.hpp"
#include "temporary_directory.hpp"

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <format>
#include <initializer_list>
#include <string>
#include <string_view>

using irama_studio::Settings;

TEST_CASE("studio.ini reads back every setting it was written with", "[studio]") {
    const Settings settings{
        .window_width = 1600,
        .window_height = 900,
        .window_maximized = true,
        .font_size = 18,
        .octave = 2,
    };
    CHECK(irama_studio::parse_settings(irama_studio::format_settings(settings)) == settings);
}

TEST_CASE("studio.ini holds one key = value line for each setting", "[studio]") {
    CHECK(irama_studio::format_settings(Settings{}) == "window_width = 1280\n"
                                                       "window_height = 800\n"
                                                       "window_maximised = false\n"
                                                       "font_size = 15\n"
                                                       "octave = 4\n");
}

TEST_CASE("studio.ini ignores keys it doesn't know", "[studio]") {
    const Settings settings = irama_studio::parse_settings("version = 3\n"
                                                           "window_width = 1000\n"
                                                           "theme = light\n"
                                                           "not a setting\n"
                                                           "font_size = 12\n");
    CHECK(settings == Settings{.window_width = 1000, .font_size = 12});
}

TEST_CASE("a bad value in studio.ini leaves that setting alone at its default", "[studio]") {
    const std::string good_lines = "window_width = 1000\n"
                                   "window_height = 700\n"
                                   "window_maximised = true\n"
                                   "font_size = 20\n"
                                   "octave = 6\n";
    const Settings good = irama_studio::parse_settings(good_lines);
    REQUIRE(good == Settings{
                        .window_width = 1000,
                        .window_height = 700,
                        .window_maximized = true,
                        .font_size = 20,
                        .octave = 6,
                    });

    // The good settings, with one of them back at its default.
    const Settings defaults;
    const auto good_but = [&]<typename T>(T Settings::* member) {
        Settings settings = good;
        settings.*member = defaults.*member;
        return settings;
    };
    // Each bad line comes after the good ones, so it overrides the good value of its own key alone.
    const auto check = [&](std::string_view key, std::initializer_list<std::string_view> values,
                           const Settings& expected) {
        for (const std::string_view value : values) {
            CAPTURE(key, value);
            const std::string text = std::format("{}{} = {}\n", good_lines, key, value);
            CHECK(irama_studio::parse_settings(text) == expected);
        }
    };
    check("window_width", {"wide", "639", "16385", "", "1000.5"},
          good_but(&Settings::window_width));
    check("window_height", {"800px", "399", "16385", "-700"}, good_but(&Settings::window_height));
    check("window_maximised", {"yes", "1", "TRUE", ""}, good_but(&Settings::window_maximized));
    check("font_size", {"9", "25", "15.5", "", "big"}, good_but(&Settings::font_size));
    check("octave", {"-1", "9", "4.5", "", "high"}, good_but(&Settings::octave));
}

TEST_CASE("studio.ini reads spaces, tabs and Windows line endings around its keys and values",
          "[studio]") {
    const Settings settings =
        irama_studio::parse_settings("  window_width\t=\t1100  \r\nfont_size=11\r\n\r\n");
    CHECK(settings == Settings{.window_width = 1100, .font_size = 11});
}

TEST_CASE("the settings are the defaults when studio.ini is missing", "[studio]") {
    const irama_studio_tests::TemporaryDirectory directory;
    CHECK(irama_studio::read_settings(directory.path() / "studio.ini") == Settings{});
}

TEST_CASE("the settings read from a studio.ini file are the ones written to it", "[studio]") {
    const irama_studio_tests::TemporaryDirectory directory;
    const Settings settings{.window_width = 2000, .window_height = 1200, .font_size = 22};
    const auto path = directory.path() / "studio.ini";
    REQUIRE(irama_studio::write_file_atomically(path, irama_studio::format_settings(settings)));
    CHECK(irama_studio::read_settings(path) == settings);
}
