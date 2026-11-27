// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "app/settings.hpp"

#include "app/config_files.hpp"
#include "audio/device_choice.hpp"
#include "input/key_map.hpp"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

namespace irama_studio {
namespace {

constexpr std::string_view whitespace = " \t\r";

[[nodiscard]] std::string_view trim(std::string_view text) {
    const std::size_t first = text.find_first_not_of(whitespace);
    if (first == std::string_view::npos) {
        return {};
    }
    const std::size_t last = text.find_last_not_of(whitespace);
    return text.substr(first, last - first + 1);
}

// Reads a whole value as a whole number from `min` to `max`.
[[nodiscard]] std::optional<int> parse_int(std::string_view text, int min, int max) {
    int value = 0;
    const char* const last = std::to_address(text.end());
    const auto [end, error] = std::from_chars(std::to_address(text.begin()), last, value);
    if (error != std::errc{} || end != last || value < min || value > max) {
        return std::nullopt;
    }
    return value;
}

[[nodiscard]] std::optional<std::uint32_t> parse_buffer_size(std::string_view text) {
    const std::optional<int> value = parse_int(text, 0, static_cast<int>(buffer_sizes.back()));
    if (!value || !std::ranges::contains(buffer_sizes, static_cast<std::uint32_t>(*value))) {
        return std::nullopt;
    }
    return static_cast<std::uint32_t>(*value);
}

[[nodiscard]] std::optional<bool> parse_bool(std::string_view text) {
    if (text == "true") {
        return true;
    }
    if (text == "false") {
        return false;
    }
    return std::nullopt;
}

// Sets one setting from its key and value, if the key is known and the value good.
void apply(Settings& settings, std::string_view key, std::string_view value) {
    if (key == "window_width") {
        settings.window_width =
            parse_int(value, min_window_width, max_window_size).value_or(Settings{}.window_width);
    } else if (key == "window_height") {
        settings.window_height =
            parse_int(value, min_window_height, max_window_size).value_or(Settings{}.window_height);
    } else if (key == "window_maximised") {
        settings.window_maximized = parse_bool(value).value_or(Settings{}.window_maximized);
    } else if (key == "font_size") {
        settings.font_size =
            parse_int(value, min_font_size, max_font_size).value_or(Settings{}.font_size);
    } else if (key == "audio_device") {
        settings.audio_device = value;
    } else if (key == "buffer_size") {
        settings.buffer_size = parse_buffer_size(value).value_or(Settings{}.buffer_size);
    } else if (key == "octave") {
        settings.octave = parse_int(value, min_octave, max_octave).value_or(Settings{}.octave);
    }
}

} // namespace

Settings parse_settings(std::string_view text) {
    Settings settings;
    while (!text.empty()) {
        const std::size_t end = text.find('\n');
        const std::string_view line = text.substr(0, end);
        text = end == std::string_view::npos ? std::string_view{} : text.substr(end + 1);
        const std::size_t equals = line.find('=');
        if (equals != std::string_view::npos) {
            apply(settings, trim(line.substr(0, equals)), trim(line.substr(equals + 1)));
        }
    }
    return settings;
}

std::string format_settings(const Settings& settings) {
    // An empty value is the system default, written with no trailing space.
    const std::string audio_device =
        settings.audio_device.empty() ? std::string{} : " " + settings.audio_device;
    return std::format("window_width = {}\n"
                       "window_height = {}\n"
                       "window_maximised = {}\n"
                       "font_size = {}\n"
                       "audio_device ={}\n"
                       "buffer_size = {}\n"
                       "octave = {}\n",
                       settings.window_width, settings.window_height, settings.window_maximized,
                       settings.font_size, audio_device, settings.buffer_size, settings.octave);
}

Settings read_settings(const std::filesystem::path& path) {
    return parse_settings(read_text_file(path).value_or(std::string{}));
}

} // namespace irama_studio
