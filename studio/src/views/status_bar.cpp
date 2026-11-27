// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "views/status_bar.hpp"

#include "input/key_map.hpp"
#include "ui/flags.hpp"

#include <imgui.h>
#include <imgui_internal.h>

namespace irama_studio {
namespace {

constexpr ImVec4 clip_color{1.0F, 0.25F, 0.25F, 1.0F};

// The keyboard octave, between a button that lowers it and one that raises it.
void draw_octave(StatusBar& bar) {
    bar.octave_step = 0;
    ImGui::TextUnformatted("Octave");
    ImGui::BeginDisabled(bar.octave <= min_octave);
    if (ImGui::SmallButton("-")) {
        bar.octave_step = -1;
    }
    ImGui::EndDisabled();
    ImGui::Text("%d", bar.octave);
    ImGui::BeginDisabled(bar.octave >= max_octave);
    if (ImGui::SmallButton("+")) {
        bar.octave_step = 1;
    }
    ImGui::EndDisabled();
}

} // namespace

void draw_status_bar(StatusBar& bar) {
    // A side bar of the main viewport, as ImGui's own main menu bar is, holding a menu bar, which
    // gives it the menu bar's look and height.
    const int flags = combine_flags(ImGuiWindowFlags_NoScrollbar, ImGuiWindowFlags_NoSavedSettings,
                                    ImGuiWindowFlags_MenuBar);
    if (ImGui::BeginViewportSideBar("##status_bar", ImGui::GetMainViewport(), ImGuiDir_Down,
                                    ImGui::GetFrameHeight(), flags) &&
        ImGui::BeginMenuBar()) {
        draw_octave(bar);
        ImGui::Separator();
        ImGui::Text("Voices %u", bar.voices);
        ImGui::Separator();
        ImGui::Text("CPU %.0f%%", static_cast<double>(bar.cpu_peak) * 100.0);
        if (bar.clipping) {
            ImGui::Separator();
            ImGui::TextColored(clip_color, "CLIP");
        }
        ImGui::EndMenuBar();
    }
    ImGui::End();
}

} // namespace irama_studio
