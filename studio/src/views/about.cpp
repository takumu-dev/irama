// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "views/about.hpp"

#include "ui/flags.hpp"

#include <imgui.h>

namespace irama_studio {

void draw_about(bool& open) {
    if (!open) {
        return;
    }
    // It floats over the docked windows, in the middle of the main window, sized to its text.
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing,
                            ImVec2{0.5F, 0.5F});
    const int flags = combine_flags(ImGuiWindowFlags_AlwaysAutoResize, ImGuiWindowFlags_NoCollapse,
                                    ImGuiWindowFlags_NoDocking, ImGuiWindowFlags_NoSavedSettings);
    if (ImGui::Begin("About Irama Studio", &open, flags)) {
        ImGui::TextUnformatted("Irama Studio");
        ImGui::TextUnformatted("Version " IRAMA_VERSION_STRING);
        ImGui::TextUnformatted("Licensed under GPL-3.0-or-later");
        ImGui::TextLinkOpenURL("https://github.com/takumu-dev/irama");
        ImGui::Separator();
        ImGui::TextUnformatted("Third-party libraries and their licences: THIRD_PARTY_NOTICES.md");
    }
    ImGui::End();
}

} // namespace irama_studio
