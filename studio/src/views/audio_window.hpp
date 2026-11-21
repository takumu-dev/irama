// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

namespace irama_studio {

class AudioEngine;

/// The Audio window's title, which also names it to the dock layout.
inline constexpr const char* audio_window_title = "Audio";

/// Draws the Audio window while `open`, which its close button clears: what the engine's device
/// runs with, or why it is silent.
void draw_audio_window(bool& open, const AudioEngine& engine);

} // namespace irama_studio
