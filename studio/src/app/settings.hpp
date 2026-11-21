// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace irama_studio {

/// The smallest window Studio opens or lets the user make, in logical pixels.
inline constexpr int min_window_width = 640;
inline constexpr int min_window_height = 400;
/// The largest window size Studio reads from its settings, in logical pixels.
inline constexpr int max_window_size = 16384;
/// The range of the UI font's size, in logical pixels.
inline constexpr int min_font_size = 10;
inline constexpr int max_font_size = 24;

/// What Studio remembers between runs, in `studio.ini`. Each member starts at its default.
struct Settings {
    /// The window's size when it isn't maximised, in logical pixels.
    int window_width = 1280;
    int window_height = 800;
    /// Whether the window opens maximised.
    bool window_maximized = false;
    /// The UI font's size, in logical pixels, before the display's scale.
    int font_size = 15;

    friend bool operator==(const Settings&, const Settings&) = default;
};

/// Reads the text of `studio.ini`: lines of `key = value`. A key Studio doesn't know is ignored,
/// and a value it can't read, or one out of its range, leaves its setting at the default without
/// touching the others. Where a key comes twice, the last one counts.
[[nodiscard]] Settings parse_settings(std::string_view text);

/// Writes the settings as the text of `studio.ini`, which `parse_settings()` reads back.
[[nodiscard]] std::string format_settings(const Settings& settings);

/// Reads the settings from a `studio.ini`. A file that is missing or can't be read gives the
/// defaults.
[[nodiscard]] Settings read_settings(const std::filesystem::path& path);

} // namespace irama_studio
