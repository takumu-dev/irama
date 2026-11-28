// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "doc/document.hpp"
#include "views/instrument_layout.hpp"
#include <irama/engine/param/param_descriptor.hpp>
#include <irama/engine/param/param_id.hpp>

#include <array>
#include <cstddef>
#include <optional>
#include <vector>

namespace irama_studio {

class AudioEngine;
class SongEditor;

/// The Instrument window's title, which also names it to the dock layout.
inline constexpr const char* instrument_window_title = "Instrument";

/// The instrument number the Instrument window edits.
inline constexpr std::size_t edited_instrument = 0x01;

/// The Instrument window: a widget for each of the instrument's parameters, built from their
/// descriptors, under one collapsing header for each part of the instrument.
///
/// It shows instrument 01 of the Document, and each change is an edit, which reaches the audio
/// engine at once. A slider moves in normalised values and shows the plain value as text, and its
/// drag from press to release is one step of the undo history. Double-clicking a slider sets its
/// default, and Ctrl-clicking one lets the user type a value.
class InstrumentWindow {
  public:
    /// Lays out the window for the instrument's parameters.
    InstrumentWindow();

    /// Draws the window while `open`, which its close button clears.
    ///
    /// \param open Whether the window is shown.
    /// \param editor The song, whose instrument the window shows and edits.
    /// \param engine The audio engine, which each edit sends the instrument to.
    void draw(bool& open, SongEditor& editor, AudioEngine& engine);

  private:
    // Draws a collapsing header of widgets for each part of the instrument, and edits the values
    // the user gives them.
    void draw_groups(const Instrument& instrument, SongEditor& editor, AudioEngine& engine);

    // Draws one parameter's widget, and returns the value the user gave it this frame, if any.
    [[nodiscard]] std::optional<float> draw_param(const irama::ParamDescriptor& param, float value);

    // Draws the text field that a Ctrl-click put in place of a slider, and returns the value typed
    // once the user presses Enter.
    [[nodiscard]] std::optional<float> draw_typing(const irama::ParamDescriptor& param);

    // Starts typing a value for the parameter, in place of its slider from the next frame.
    void start_typing(const irama::ParamDescriptor& param, float value);

    std::vector<ParamGroup> groups_;
    // The parameter whose value is being typed, and the text so far.
    std::optional<irama::ParamId> typing_;
    std::array<char, 64> typed_{};
    bool focus_typing_ = false;
    // The slider a double-click has just reset, while the mouse still holds it.
    std::optional<irama::ParamId> held_after_reset_;
};

} // namespace irama_studio
