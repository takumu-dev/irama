// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "input/shortcut_table.hpp"
#include "views/audio_window.hpp"
#include "views/instrument_window.hpp"

#include <optional>

namespace irama_studio {

class AudioEngine;
class SongEditor;
struct StatusBar;

/// Everything Studio's main window shows: the main menu bar, the dock space that fills the window
/// with the windows docked in it, the status bar, and the About window.
class Workspace {
  public:
    /// Draws one frame of the workspace, and returns false once the user has chosen to quit.
    ///
    /// \param status What the status bar shows, and where it puts what the user asks of it.
    /// \param engine The audio engine, which the Audio window describes and every edit sends the
    ///               parts it changes.
    /// \param editor The song, which the Instrument window edits and the Edit menu steps through.
    [[nodiscard]] bool draw(StatusBar& status, AudioEngine& engine, SongEditor& editor);

    /// \return The Audio window, with what the user asked of it on the latest frame.
    [[nodiscard]] const AudioWindow& audio_window() const { return audio_window_; }

    /// \return The shortcut whose menu entry the user chose on the latest frame, if any.
    [[nodiscard]] std::optional<ShortcutId> chosen_shortcut() const { return chosen_shortcut_; }

  private:
    InstrumentWindow instrument_;
    bool instrument_open_ = true;
    bool audio_open_ = true;
    bool about_open_ = false;
    AudioWindow audio_window_;
    std::optional<ShortcutId> chosen_shortcut_;
};

} // namespace irama_studio
