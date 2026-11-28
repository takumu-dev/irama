// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "views/workspace.hpp"

#include "app/song_editor.hpp"
#include "doc/undo_history.hpp"
#include "input/key_map.hpp"
#include "input/shortcut_table.hpp"
#include "views/about.hpp"
#include "views/audio_window.hpp"
#include "views/instrument_window.hpp"
#include "views/status_bar.hpp"

#include <imgui.h>
#include <imgui_internal.h>

#include <optional>
#include <string>
#include <string_view>

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

// A menu entry for a shortcut, showing its name and its chords from the shortcut table, and
// returns whether the user chose it.
[[nodiscard]] bool shortcut_menu_item(ShortcutId id, bool enabled) {
    const Shortcut& shortcut = find_shortcut(id);
    const std::string name{shortcut.name};
    return ImGui::MenuItem(name.c_str(), chords_text(shortcut).c_str(), false, enabled);
}

// The Edit menu's entry for undo or redo, named after the step it would take, as in "Undo Set
// Cutoff", and returns whether the user chose it. Its ID stays the same whatever the step.
[[nodiscard]] bool step_menu_item(ShortcutId id, std::optional<std::string_view> step) {
    const Shortcut& shortcut = find_shortcut(id);
    std::string label{shortcut.name};
    if (step) {
        label += ' ';
        label += *step;
    }
    label += "###";
    label += shortcut.name;
    return ImGui::MenuItem(label.c_str(), chords_text(shortcut).c_str(), false, step.has_value());
}

// The Edit menu, and the shortcut the user chose from it, if any.
[[nodiscard]] std::optional<ShortcutId> draw_edit_menu(const UndoHistory& history) {
    std::optional<ShortcutId> chosen;
    if (ImGui::BeginMenu("Edit")) {
        if (step_menu_item(ShortcutId::undo, history.undo_name())) {
            chosen = ShortcutId::undo;
        }
        if (step_menu_item(ShortcutId::redo, history.redo_name())) {
            chosen = ShortcutId::redo;
        }
        ImGui::EndMenu();
    }
    return chosen;
}

} // namespace

bool Workspace::draw(StatusBar& status, AudioEngine& engine, SongEditor& editor) {
    bool keep_running = true;
    chosen_shortcut_.reset();
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Quit")) {
                keep_running = false;
            }
            ImGui::EndMenu();
        }
        if (const std::optional<ShortcutId> chosen = draw_edit_menu(editor.history())) {
            chosen_shortcut_ = chosen;
        }
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem(instrument_window_title, nullptr, &instrument_open_);
            ImGui::MenuItem(audio_window_title, nullptr, &audio_open_);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Keyboard")) {
            if (shortcut_menu_item(ShortcutId::lower_octave, status.octave > min_octave)) {
                chosen_shortcut_ = ShortcutId::lower_octave;
            }
            if (shortcut_menu_item(ShortcutId::raise_octave, status.octave < max_octave)) {
                chosen_shortcut_ = ShortcutId::raise_octave;
            }
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
    draw_status_bar(status);

    // The menu bar and the status bar have taken their space, so the layout fills what is left.
    const ImGuiID dock_space = ImGui::GetID("workspace");
    if (ImGui::DockBuilderGetNode(dock_space) == nullptr) {
        build_default_layout(dock_space);
    }
    ImGui::DockSpaceOverViewport(dock_space);

    instrument_.draw(instrument_open_, editor, engine);
    draw_audio_window(audio_open_, audio_window_, engine);
    draw_about(about_open_);
    return keep_running;
}

} // namespace irama_studio
