// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <span>

namespace irama_studio {

/// Inter Regular, the UI font, as the bytes of its TTF file, which the build compiles in.
[[nodiscard]] std::span<const unsigned char> inter_regular_ttf();

/// Loads the UI font into the current ImGui context, as its default font, at `size` logical pixels.
void load_fonts(float size);

} // namespace irama_studio
