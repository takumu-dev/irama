// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace irama_studio {

class AudioEngine;

/// The Audio window's title, which also names it to the dock layout.
inline constexpr const char* audio_window_title = "Audio";

/// What the Audio window keeps between frames, and what the user asks of it there.
struct AudioWindow {
    /// The devices there were when the Device combo last opened.
    std::vector<std::string> device_names;
    /// The device the user picked: a device's name, or empty for the system default. Drawing sets
    /// it.
    std::optional<std::string> chosen_device;
    /// The buffer size the user picked, in frames. Drawing sets it.
    std::optional<std::uint32_t> chosen_buffer_size;
    /// Whether the user pressed Reopen. Drawing sets it.
    bool reopen = false;

    /// \return Whether the user asked for the device to open again.
    [[nodiscard]] bool wants_open() const { return chosen_device || chosen_buffer_size || reopen; }
};

/// Draws the Audio window while `open`, which its close button clears: the backend, the Device and
/// Buffer size combos, what the engine's device runs with or that it is silent, why, and Reopen
/// while silent. The devices are listed again each time the Device combo opens.
void draw_audio_window(bool& open, AudioWindow& window, const AudioEngine& engine);

} // namespace irama_studio
