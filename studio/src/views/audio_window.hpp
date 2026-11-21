// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

namespace irama_studio {

/// The Audio window's title, which also names it to the dock layout.
inline constexpr const char* audio_window_title = "Audio";

/// Draws the Audio window while `open`, which its close button clears.
void draw_audio_window(bool& open);

} // namespace irama_studio
