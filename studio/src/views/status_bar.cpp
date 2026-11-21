// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "views/status_bar.hpp"

#include "ui/flags.hpp"

#include <imgui.h>
#include <imgui_internal.h>

namespace irama_studio {

void draw_status_bar() {
    // A side bar of the main viewport, as ImGui's own main menu bar is, holding a menu bar, which
    // gives it the menu bar's look and height.
    const int flags = combine_flags(ImGuiWindowFlags_NoScrollbar, ImGuiWindowFlags_NoSavedSettings,
                                    ImGuiWindowFlags_MenuBar);
    if (ImGui::BeginViewportSideBar("##status_bar", ImGui::GetMainViewport(), ImGuiDir_Down,
                                    ImGui::GetFrameHeight(), flags) &&
        ImGui::BeginMenuBar()) {
        ImGui::EndMenuBar();
    }
    ImGui::End();
}

} // namespace irama_studio
