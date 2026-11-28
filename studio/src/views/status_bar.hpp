// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "input/key_map.hpp"

#include <cstdint>
#include <string>

namespace irama_studio {

/// What the status bar shows, and what the user asks of it there.
struct StatusBar {
    /// The keyboard octave.
    int octave = default_octave;
    /// How many voices are sounding.
    std::uint32_t voices = 0;
    /// The audio callback's recent peak load, as a share of its period.
    float cpu_peak = 0.0F;
    /// Whether to show CLIP.
    bool clipping = false;
    /// Why the latest edit was refused, or empty.
    std::string refusal;
    /// How far the user moved the keyboard octave with the status bar's buttons, −1, 0 or +1.
    /// Drawing sets it.
    int octave_step = 0;
};

/// Draws the status bar along the bottom of the main window: the keyboard octave, with buttons
/// to lower and raise it, the voice count, the CPU meter, CLIP after a clipped sample, and why the
/// latest edit was refused. It takes its height from the space the dock layout fills, so it is
/// drawn before the dock space.
void draw_status_bar(StatusBar& bar);

} // namespace irama_studio
