// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

namespace irama_studio {

/// The Instrument window's title, which also names it to the dock layout.
inline constexpr const char* instrument_window_title = "Instrument";

/// Draws the Instrument window while `open`, which its close button clears.
void draw_instrument_window(bool& open);

} // namespace irama_studio
