// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

namespace irama_studio {

/// Gives the current ImGui context Studio's style, ImGui's own dark style, with its sizes and its
/// fonts scaled for the display. A fresh style is scaled each time, so calling it again when the
/// scale changes never compounds the scales.
void apply_theme(float scale);

} // namespace irama_studio
