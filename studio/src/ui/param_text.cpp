// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/param_text.hpp"

#include <irama/engine/param/param_descriptor.hpp>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

namespace irama_studio {
namespace {

// The symbol written after a value in the unit, with the space before it where it takes one.
[[nodiscard]] std::string_view unit_suffix(irama::ParamUnit unit) {
    switch (unit) {
    case irama::ParamUnit::none:
        return "";
    case irama::ParamUnit::hertz:
        return " Hz";
    case irama::ParamUnit::decibels:
        return " dB";
    case irama::ParamUnit::seconds:
        return " s";
    case irama::ParamUnit::milliseconds:
        return " ms";
    case irama::ParamUnit::percent:
        return "%";
    case irama::ParamUnit::semitones:
        return " st";
    case irama::ParamUnit::cents:
        return " ct";
    }
    return "";
}

// Whether a value in the unit is an offset either way, and so is written with its sign.
[[nodiscard]] bool signed_unit(irama::ParamUnit unit) {
    return unit == irama::ParamUnit::semitones || unit == irama::ParamUnit::cents;
}

[[nodiscard]] bool is_space(char c) { return c == ' ' || c == '\t'; }

[[nodiscard]] std::string_view trim(std::string_view text) {
    while (!text.empty() && is_space(text.front())) {
        text.remove_prefix(1);
    }
    while (!text.empty() && is_space(text.back())) {
        text.remove_suffix(1);
    }
    return text;
}

[[nodiscard]] char lower(char c) {
    return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
}

[[nodiscard]] bool equal_ignoring_case(std::string_view a, std::string_view b) {
    return std::ranges::equal(a, b, [](char x, char y) { return lower(x) == lower(y); });
}

} // namespace

std::string format_param_value(const irama::ParamDescriptor& param, float value) {
    const bool stepped = param.scale == irama::ParamScale::stepped;
    if (stepped && !param.labels.empty()) {
        const auto step = static_cast<std::ptrdiff_t>(std::lround(value - param.min));
        const auto last = static_cast<std::ptrdiff_t>(param.labels.size()) - 1;
        const auto index = static_cast<std::size_t>(std::clamp<std::ptrdiff_t>(step, 0, last));
        return std::string{param.labels.subspan(index, 1).front()};
    }
    const int precision = stepped ? 0 : param.precision;
    std::string number = std::format("{:.{}f}", value, precision);
    const bool zero = number.find_first_not_of("-0.") == std::string::npos;
    if (zero && number.starts_with('-')) {
        number.erase(0, 1);
    } else if (!zero && signed_unit(param.unit) && !number.starts_with('-')) {
        number.insert(0, 1, '+');
    }
    return number.append(unit_suffix(param.unit));
}

std::optional<float> parse_param_value(const irama::ParamDescriptor& param, std::string_view text) {
    text = trim(text);
    for (float step = param.min; const std::string_view label : param.labels) {
        if (equal_ignoring_case(text, label)) {
            return step;
        }
        ++step;
    }
    const std::string_view symbol = trim(unit_suffix(param.unit));
    if (!symbol.empty() && text.size() >= symbol.size() &&
        equal_ignoring_case(text.substr(text.size() - symbol.size()), symbol)) {
        text = trim(text.substr(0, text.size() - symbol.size()));
    }
    // from_chars takes a minus sign but not a plus.
    if (text.starts_with('+') && !text.substr(1).starts_with('-')) {
        text.remove_prefix(1);
    }
    float value = 0.0F;
    const char* const last = std::to_address(text.end());
    const auto [end, error] = std::from_chars(std::to_address(text.begin()), last, value);
    if (error != std::errc{} || end != last || std::isnan(value)) {
        return std::nullopt;
    }
    return param.constrain(value);
}

} // namespace irama_studio
