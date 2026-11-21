// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "app/options.hpp"

#include <charconv>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>

namespace irama_studio {
namespace {

constexpr std::string_view bad_frame_count =
    "irama-studio: --exit-after-frames takes a whole number of frames above 0";

// Reads a whole argument as a count above zero.
[[nodiscard]] std::optional<std::uint64_t> parse_count(std::string_view text) {
    std::uint64_t value = 0;
    const char* const last = std::to_address(text.end());
    const auto [end, error] = std::from_chars(std::to_address(text.begin()), last, value);
    if (error != std::errc{} || end != last || value == 0) {
        return std::nullopt;
    }
    return value;
}

} // namespace

std::expected<Options, std::string> parse_options(std::span<const std::string_view> arguments) {
    Options options;
    while (!arguments.empty()) {
        const std::string_view name = arguments.front();
        arguments = arguments.subspan(1);
        if (name != "--exit-after-frames") {
            return std::unexpected{std::string{usage}};
        }
        const std::optional<std::uint64_t> count =
            arguments.empty() ? std::nullopt : parse_count(arguments.front());
        if (!count) {
            return std::unexpected{std::string{bad_frame_count}};
        }
        arguments = arguments.subspan(1);
        options.exit_after_frames = count;
    }
    return options;
}

} // namespace irama_studio
