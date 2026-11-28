// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include "app/config_files.hpp"

#include "temporary_directory.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>

using irama_studio::DelayedFile;
using irama_studio::Environment;
using std::filesystem::path;

TEST_CASE("the settings directory is irama in XDG_CONFIG_HOME when it is set", "[studio]") {
    const Environment environment{.xdg_config_home = "/home/ana/settings", .home = "/home/ana"};
    CHECK(irama_studio::config_directory(std::nullopt, environment) ==
          path{"/home/ana/settings/irama"});
}

TEST_CASE("the settings directory is .config/irama in HOME without XDG_CONFIG_HOME", "[studio]") {
    const Environment environment{.home = "/home/ana"};
    CHECK(irama_studio::config_directory(std::nullopt, environment) ==
          path{"/home/ana/.config/irama"});
}

TEST_CASE("a relative XDG_CONFIG_HOME is ignored, as the XDG specification says", "[studio]") {
    const Environment environment{.xdg_config_home = "settings", .home = "/home/ana"};
    CHECK(irama_studio::config_directory(std::nullopt, environment) ==
          path{"/home/ana/.config/irama"});
}

TEST_CASE("the settings directory is irama in APPDATA when it is set", "[studio]") {
    const Environment environment{.appdata = R"(C:\Users\ana\AppData\Roaming)"};
    CHECK(irama_studio::config_directory(std::nullopt, environment) ==
          path{R"(C:\Users\ana\AppData\Roaming)"} / "irama");
}

TEST_CASE("the settings directory is the one --config-dir gives, whatever the environment",
          "[studio]") {
    const Environment xdg{.xdg_config_home = "/home/ana/settings", .home = "/home/ana"};
    const Environment windows{.appdata = R"(C:\Users\ana\AppData\Roaming)"};
    CHECK(irama_studio::config_directory(path{"/tmp/clean"}, xdg) == path{"/tmp/clean"});
    CHECK(irama_studio::config_directory(path{"/tmp/clean"}, windows) == path{"/tmp/clean"});
    CHECK(irama_studio::config_directory(path{"/tmp/clean"}, Environment{}) == path{"/tmp/clean"});
}

TEST_CASE("there is no settings directory without --config-dir, APPDATA, XDG_CONFIG_HOME or HOME",
          "[studio]") {
    CHECK_FALSE(irama_studio::config_directory(std::nullopt, Environment{}).has_value());
}

TEST_CASE("a missing file reads as nothing", "[studio]") {
    const irama_studio_tests::TemporaryDirectory directory;
    CHECK_FALSE(irama_studio::read_text_file(directory.path() / "imgui.ini").has_value());
}

TEST_CASE("a file written atomically holds the new text, and leaves no temporary file",
          "[studio]") {
    const irama_studio_tests::TemporaryDirectory directory;
    const path file = directory.path() / "irama" / "imgui.ini";
    REQUIRE(irama_studio::write_file_atomically(file, "first\n"));
    REQUIRE(irama_studio::write_file_atomically(file, "second\n"));
    CHECK(irama_studio::read_text_file(file) == "second\n");
    CHECK_FALSE(std::filesystem::exists(directory.path() / "irama" / "imgui.ini.tmp"));
}

TEST_CASE("a file is written through a temporary file, so a failed write leaves the old text",
          "[studio]") {
    const irama_studio_tests::TemporaryDirectory directory;
    const path file = directory.path() / "studio.ini";
    REQUIRE(irama_studio::write_file_atomically(file, "old\n"));
    // A directory where the temporary file goes makes the write fail before the file is touched.
    std::filesystem::create_directory(directory.path() / "studio.ini.tmp");
    const auto written = irama_studio::write_file_atomically(file, "new\n");
    CHECK_FALSE(written.has_value());
    CHECK(irama_studio::read_text_file(file) == "old\n");
}

TEST_CASE("a delayed file is written a second after its last change, and not before", "[studio]") {
    const irama_studio_tests::TemporaryDirectory directory;
    const path file = directory.path() / "imgui.ini";
    DelayedFile delayed{file, ""};
    const DelayedFile::Clock::time_point start{};
    constexpr auto half = std::chrono::milliseconds{500};

    delayed.change("a\n", start);
    delayed.change("ab\n", start + half);
    REQUIRE(delayed.write_if_due(start + DelayedFile::delay));
    CHECK_FALSE(std::filesystem::exists(file));

    REQUIRE(delayed.write_if_due(start + half + DelayedFile::delay));
    CHECK(irama_studio::read_text_file(file) == "ab\n");
}

TEST_CASE("a delayed file isn't written when its text comes back to what it holds", "[studio]") {
    const irama_studio_tests::TemporaryDirectory directory;
    const path file = directory.path() / "imgui.ini";
    DelayedFile delayed{file, "kept\n"};
    const DelayedFile::Clock::time_point start{};

    delayed.change("changed\n", start);
    delayed.change("kept\n", start);
    REQUIRE(delayed.write_if_due(start + DelayedFile::delay));
    REQUIRE(delayed.flush());
    CHECK_FALSE(std::filesystem::exists(file));
}

TEST_CASE("a delayed file is written at once when flushed, as on quitting", "[studio]") {
    const irama_studio_tests::TemporaryDirectory directory;
    const path file = directory.path() / "studio.ini";
    DelayedFile delayed{file, ""};

    delayed.change("font_size = 12\n", DelayedFile::Clock::time_point{});
    REQUIRE(delayed.flush());
    CHECK(irama_studio::read_text_file(file) == "font_size = 12\n");
}
