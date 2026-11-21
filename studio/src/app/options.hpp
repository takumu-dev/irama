// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace irama_studio {

/// The usage line, which Studio prints when it doesn't understand its command line.
inline constexpr std::string_view usage = "usage: irama-studio [--exit-after-frames <count>]";

/// What Studio's command line asks of it.
struct Options {
    /// The number of frames to draw before quitting, as a test that starts Studio asks for, or
    /// none to run until the user quits.
    std::optional<std::uint64_t> exit_after_frames;
};

/// Reads Studio's command line, the arguments after the program's name. If it can't, the error
/// is the text to print: the usage, or what is wrong with an option's value.
[[nodiscard]] std::expected<Options, std::string>
parse_options(std::span<const std::string_view> arguments);

} // namespace irama_studio
