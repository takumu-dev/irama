// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "views/instrument_window.hpp"

#include "app/song_editor.hpp"
#include "doc/document.hpp"
#include "doc/edits.hpp"
#include "ui/flags.hpp"
#include "ui/param_text.hpp"
#include "views/instrument_layout.hpp"
#include <irama/engine/base/narrow.hpp>
#include <irama/engine/param/param_descriptor.hpp>

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace irama_studio {
namespace {

// ImGui's own Ctrl-click would type a normalised value, so the window takes Ctrl-clicks itself.
// The slider's format is the plain value's text, so ImGui has no precision to round to.
[[nodiscard]] int slider_flags() {
    return combine_flags(ImGuiSliderFlags_NoInput, ImGuiSliderFlags_NoRoundToFormat,
                         ImGuiSliderFlags_AlwaysClamp);
}

[[nodiscard]] int typing_flags() {
    return combine_flags(ImGuiInputTextFlags_EnterReturnsTrue, ImGuiInputTextFlags_AutoSelectAll);
}

// A slider shows its text through a printf format, in which a literal % is written %%.
[[nodiscard]] std::string as_format(std::string_view text) {
    std::string format;
    for (const char c : text) {
        format.push_back(c);
        if (c == '%') {
            format.push_back('%');
        }
    }
    return format;
}

[[nodiscard]] int whole(float value) { return irama::narrow<int>(std::lround(value)); }

// Draws a combo box of a stepped parameter's labels, and returns the value of the label chosen.
[[nodiscard]] std::optional<float> draw_combo(const irama::ParamDescriptor& param, float value) {
    std::optional<float> chosen;
    const std::string name{param.name};
    const std::string preview = format_param_value(param, value);
    if (ImGui::BeginCombo(name.c_str(), preview.c_str())) {
        for (float option = param.min; const std::string_view name_of_step : param.labels) {
            const std::string label{name_of_step};
            if (ImGui::Selectable(label.c_str(), whole(option) == whole(value))) {
                chosen = option;
            }
            ++option;
        }
        ImGui::EndCombo();
    }
    return chosen;
}

} // namespace

InstrumentWindow::InstrumentWindow() : groups_(group_params(instrument_table())) {}

void InstrumentWindow::draw(bool& open, SongEditor& editor, AudioEngine& engine) {
    if (!open) {
        return;
    }
    if (ImGui::Begin(instrument_window_title, &open)) {
        const std::optional<Instrument>& instrument =
            editor.document().instrument(edited_instrument);
        if (!instrument) {
            ImGui::TextDisabled("Instrument %02zX is empty", edited_instrument);
        } else {
            draw_groups(*instrument, editor, engine);
        }
    }
    ImGui::End();
}

void InstrumentWindow::draw_groups(const Instrument& instrument, SongEditor& editor,
                                   AudioEngine& engine) {
    for (const ParamGroup& group : groups_) {
        const std::string title{group.title};
        if (!ImGui::CollapsingHeader(title.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            continue;
        }
        for (const irama::ParamDescriptor* const param : group.params) {
            ImGui::PushID(param);
            // A slider's drag is one gesture, from press to release. A label chosen or a value
            // typed is a gesture of its own.
            const bool one_shot = typing_ == param->id || widget_for(*param) == ParamWidget::combo;
            const std::optional<float> changed = draw_param(*param, instrument.value(param->id));
            const bool released = ImGui::IsItemDeactivated();
            if (changed) {
                editor.apply(set_instrument_value(edited_instrument, param->id, *changed), engine);
            }
            if (released || (changed && one_shot)) {
                editor.end_gesture();
            }
            ImGui::PopID();
        }
    }
}

std::optional<float> InstrumentWindow::draw_param(const irama::ParamDescriptor& param,
                                                  float value) {
    if (typing_ == param.id) {
        return draw_typing(param);
    }
    const std::string name{param.name};
    const std::string format = as_format(format_param_value(param, value));
    std::optional<float> changed;
    switch (widget_for(param)) {
    case ParamWidget::combo:
        return draw_combo(param, value);
    case ParamWidget::slider: {
        float normalized = param.to_normalized(value);
        if (ImGui::SliderFloat(name.c_str(), &normalized, 0.0F, 1.0F, format.c_str(),
                               slider_flags())) {
            changed = param.from_normalized(normalized);
        }
        break;
    }
    case ParamWidget::integer_slider: {
        int step = whole(value);
        if (ImGui::SliderInt(name.c_str(), &step, whole(param.min), whole(param.max),
                             format.c_str(), slider_flags())) {
            changed = static_cast<float>(step);
        }
        break;
    }
    }

    // A click moves a slider to the mouse, so a slider reset by a double-click ignores the mouse
    // until it is let go, and one Ctrl-clicked keeps its value.
    if (held_after_reset_ == param.id) {
        if (ImGui::IsItemActive()) {
            return std::nullopt;
        }
        held_after_reset_.reset();
    }
    if (ImGui::IsItemActivated() && ImGui::GetIO().KeyCtrl) {
        start_typing(param, value);
        return std::nullopt;
    }
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        held_after_reset_ = param.id;
        return param.default_value;
    }
    return changed;
}

std::optional<float> InstrumentWindow::draw_typing(const irama::ParamDescriptor& param) {
    if (focus_typing_) {
        ImGui::SetKeyboardFocusHere();
        focus_typing_ = false;
    }
    const std::string name{param.name};
    if (ImGui::InputText(name.c_str(), typed_.data(), typed_.size(), typing_flags())) {
        typing_.reset();
        return parse_param_value(param, std::string_view{typed_.data()});
    }
    // Escape, or a click anywhere else, leaves the value as it was.
    if (ImGui::IsItemDeactivated()) {
        typing_.reset();
    }
    return std::nullopt;
}

void InstrumentWindow::start_typing(const irama::ParamDescriptor& param, float value) {
    const std::string text = format_param_value(param, value);
    typed_.fill('\0');
    const std::size_t length = std::min(text.size(), typed_.size() - 1);
    std::ranges::copy_n(text.begin(), static_cast<std::ptrdiff_t>(length), typed_.begin());
    typing_ = param.id;
    focus_typing_ = true;
}

} // namespace irama_studio
