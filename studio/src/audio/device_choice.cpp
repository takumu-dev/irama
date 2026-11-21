// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "audio/device_choice.hpp"

#include <algorithm>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace irama_studio {

DeviceChoice choose_device(std::string_view setting, std::span<const std::string> listed) {
    if (setting.empty()) {
        return {};
    }
    if (std::ranges::contains(listed, setting)) {
        return {.name = std::string{setting}};
    }
    return {.note = std::format("{} not found, using system default", setting)};
}

std::optional<DeviceChoice> fall_back_after_loss(std::string_view lost) {
    if (lost.empty()) {
        return std::nullopt;
    }
    return DeviceChoice{.note = std::format("{} lost, using system default", lost)};
}

} // namespace irama_studio
