// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

namespace irama_studio {

/// Draws the status bar along the bottom of the main window. It takes its height from the space
/// the dock layout fills, so it is drawn before the dock space.
void draw_status_bar();

} // namespace irama_studio
