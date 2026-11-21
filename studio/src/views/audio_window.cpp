// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "views/audio_window.hpp"

#include "audio/audio_device.hpp"
#include "audio/audio_engine.hpp"

#include <imgui.h>

#include <string>
#include <string_view>

namespace irama_studio {

void draw_audio_window(bool& open, const AudioEngine& engine) {
    if (!open) {
        return;
    }
    if (ImGui::Begin(audio_window_title, &open)) {
        const auto& granted = engine.device().granted();
        if (granted) {
            const std::string_view backend = backend_name(granted->backend);
            ImGui::Text("%.*s: %s", static_cast<int>(backend.size()), backend.data(),
                        granted->device_name.c_str());
            ImGui::Text("Playing at %u Hz, %u frames a callback", granted->sample_rate,
                        granted->callback_frames);
        } else {
            ImGui::TextUnformatted("Silent");
            ImGui::TextWrapped("%s", engine.last_error().c_str());
        }
    }
    ImGui::End();
}

} // namespace irama_studio
