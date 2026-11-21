// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/theme.hpp"

#include "ui/fonts.hpp"

#include <imgui.h>

namespace irama_studio {

void apply_theme(float scale) {
    ImGuiStyle style;
    ImGui::StyleColorsDark(&style);
    style.FontSizeBase = ui_font_size;
    style.ScaleAllSizes(scale);
    style.FontScaleDpi = scale;
    ImGui::GetStyle() = style;
}

} // namespace irama_studio
