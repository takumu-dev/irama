// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace irama_studio {

/// The buffer sizes Studio offers, in frames: the number of frames in each audio callback.
inline constexpr std::array<std::uint32_t, 4> buffer_sizes{128, 256, 512, 1024};

/// The buffer size Studio starts with, in frames.
inline constexpr std::uint32_t default_buffer_size = 256;

/// The name the system default device goes by, which follows the user's choice of default.
inline constexpr std::string_view system_default_name = "System default";

/// The device to open, and what to tell the user about how it was chosen.
struct DeviceChoice {
    /// The device's name, or empty for the system default.
    std::string name{};
    /// Why the system default opens in place of the device asked for, or empty.
    std::string note{};

    friend bool operator==(const DeviceChoice&, const DeviceChoice&) = default;
};

/// Chooses the device to open for the `audio_device` setting. An empty setting is the system
/// default. A named device opens by its name when it is listed, and the system default opens in its
/// place when it isn't, with a note saying so. The setting itself is kept either way.
///
/// \param setting The device's name, or empty for the system default.
/// \param listed The names of the devices there are now.
[[nodiscard]] DeviceChoice choose_device(std::string_view setting,
                                         std::span<const std::string> listed);

/// Chooses the device to open after the device `lost` stopped calling back. A named device falls
/// back to the system default, with a note saying so. The system default has nothing to fall back
/// to, so Studio goes silent.
///
/// \param lost The lost device's name, or empty for the system default.
[[nodiscard]] std::optional<DeviceChoice> fall_back_after_loss(std::string_view lost);

} // namespace irama_studio
