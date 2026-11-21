// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "views/instrument_window.hpp"

#include <imgui.h>

namespace irama_studio {

void draw_instrument_window(bool& open) {
    if (!open) {
        return;
    }
    ImGui::Begin(instrument_window_title, &open);
    ImGui::End();
}

} // namespace irama_studio
