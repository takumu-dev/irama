// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <span>

namespace irama_studio {

/// The size of the UI font, in logical pixels, before the display's scale.
inline constexpr float ui_font_size = 15.0F;

/// Inter Regular, the UI font, as the bytes of its TTF file, which the build compiles in.
[[nodiscard]] std::span<const unsigned char> inter_regular_ttf();

/// Loads the UI font into the current ImGui context, as its default font.
void load_fonts();

} // namespace irama_studio
