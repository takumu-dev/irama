// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "views/workspace.hpp"

#include "views/about.hpp"
#include "views/audio_window.hpp"
#include "views/instrument_window.hpp"
#include "views/status_bar.hpp"

#include <imgui.h>
#include <imgui_internal.h>

namespace irama_studio {
namespace {

// The share of the main window's width that the Instrument window takes, beside the Audio window.
constexpr float instrument_share = 2.0F / 3.0F;

// Lays out the dock space with no saved layout: the Instrument window on the left, and the Audio
// window on the right.
void build_default_layout(ImGuiID dock_space) {
    ImGui::DockBuilderRemoveNode(dock_space);
    ImGui::DockBuilderAddNode(dock_space, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dock_space, ImGui::GetMainViewport()->WorkSize);
    ImGuiID left = 0;
    ImGuiID right = 0;
    ImGui::DockBuilderSplitNode(dock_space, ImGuiDir_Left, instrument_share, &left, &right);
    ImGui::DockBuilderDockWindow(instrument_window_title, left);
    ImGui::DockBuilderDockWindow(audio_window_title, right);
    ImGui::DockBuilderFinish(dock_space);
}

} // namespace

bool Workspace::draw() {
    bool keep_running = true;
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Quit")) {
                keep_running = false;
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem(instrument_window_title, nullptr, &instrument_open_);
            ImGui::MenuItem(audio_window_title, nullptr, &audio_open_);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("About")) {
                about_open_ = true;
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
    draw_status_bar();

    // The menu bar and the status bar have taken their space, so the layout fills what is left.
    const ImGuiID dock_space = ImGui::GetID("workspace");
    if (ImGui::DockBuilderGetNode(dock_space) == nullptr) {
        build_default_layout(dock_space);
    }
    ImGui::DockSpaceOverViewport(dock_space);

    draw_instrument_window(instrument_open_);
    draw_audio_window(audio_open_);
    draw_about(about_open_);
    return keep_running;
}

} // namespace irama_studio
