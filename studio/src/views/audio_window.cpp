// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "views/audio_window.hpp"

#include "audio/audio_device.hpp"
#include "audio/audio_engine.hpp"
#include "audio/device_choice.hpp"

#include <imgui.h>

#include <cstdint>
#include <format>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>

namespace irama_studio {
namespace {

constexpr ImVec4 error_color{1.0F, 0.4F, 0.4F, 1.0F};
constexpr ImVec4 note_color{1.0F, 0.8F, 0.4F, 1.0F};

// A device setting as the Device combo shows it.
[[nodiscard]] std::string device_label(std::string_view setting) {
    return std::string{setting.empty() ? system_default_name : setting};
}

// A buffer size as the Buffer size combo shows it: in frames, and in milliseconds at the device's
// rate, if one is open.
[[nodiscard]] std::string buffer_size_label(std::uint32_t frames, std::uint32_t sample_rate) {
    if (sample_rate == 0) {
        return std::format("{} frames", frames);
    }
    const double milliseconds = 1000.0 * frames / sample_rate;
    return std::format("{} frames ({:.1f} ms)", frames, milliseconds);
}

void draw_device_combo(AudioWindow& window, const AudioEngine& engine) {
    const std::string& setting = engine.device_setting();
    if (!ImGui::BeginCombo("Device", device_label(setting).c_str())) {
        return;
    }
    if (ImGui::IsWindowAppearing()) {
        window.device_names = engine.device_names();
    }
    if (ImGui::Selectable(std::string{system_default_name}.c_str(), setting.empty())) {
        window.chosen_device = std::string{};
    }
    for (const auto& [index, name] : std::views::enumerate(window.device_names)) {
        // Two devices may share a name.
        ImGui::PushID(static_cast<int>(index));
        if (ImGui::Selectable(name.c_str(), name == setting)) {
            window.chosen_device = name;
        }
        ImGui::PopID();
    }
    ImGui::EndCombo();
}

void draw_buffer_size_combo(AudioWindow& window, const AudioEngine& engine,
                            std::uint32_t sample_rate) {
    const std::uint32_t current = engine.buffer_size();
    if (!ImGui::BeginCombo("Buffer size", buffer_size_label(current, sample_rate).c_str())) {
        return;
    }
    for (const std::uint32_t frames : buffer_sizes) {
        if (ImGui::Selectable(buffer_size_label(frames, sample_rate).c_str(), frames == current)) {
            window.chosen_buffer_size = frames;
        }
    }
    ImGui::EndCombo();
}

} // namespace

void draw_audio_window(bool& open, AudioWindow& window, const AudioEngine& engine) {
    window.chosen_device.reset();
    window.chosen_buffer_size.reset();
    window.reopen = false;
    if (!open) {
        return;
    }
    if (ImGui::Begin(audio_window_title, &open)) {
        const std::optional<AudioBackend> backend = engine.device().backend();
        const std::string_view backend_label = backend ? backend_name(*backend) : "None";
        ImGui::Text("Backend: %.*s", static_cast<int>(backend_label.size()), backend_label.data());

        const std::optional<AudioGranted>& granted = engine.device().granted();
        const std::uint32_t sample_rate = granted ? granted->sample_rate : 0;
        draw_device_combo(window, engine);
        draw_buffer_size_combo(window, engine, sample_rate);

        ImGui::Separator();
        if (granted) {
            ImGui::TextWrapped("Playing %s at %u Hz, %u frames a callback",
                               granted->device_name.c_str(), granted->sample_rate,
                               granted->callback_frames);
        } else {
            ImGui::TextUnformatted("Silent");
        }
        if (!engine.last_error().empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, error_color);
            ImGui::TextWrapped("%s", engine.last_error().c_str());
            ImGui::PopStyleColor();
        }
        if (!engine.note().empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, note_color);
            ImGui::TextWrapped("%s", engine.note().c_str());
            ImGui::PopStyleColor();
        }
        const std::string log_error = engine.device().last_log_error();
        if (!log_error.empty()) {
            ImGui::TextDisabled("miniaudio: %s", log_error.c_str());
        }
        if (!engine.is_open() && ImGui::Button("Reopen")) {
            window.reopen = true;
        }
    }
    ImGui::End();
}

} // namespace irama_studio
