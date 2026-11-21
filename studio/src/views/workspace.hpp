// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "views/audio_window.hpp"

namespace irama_studio {

class AudioEngine;
struct StatusBar;

/// Everything Studio's main window shows: the main menu bar, the dock space that fills the window
/// with the windows docked in it, the status bar, and the About window.
class Workspace {
  public:
    /// Draws one frame of the workspace, and returns false once the user has chosen to quit.
    ///
    /// \param status What the status bar shows, and where it puts what the user asks of it.
    /// \param engine The audio engine, which the Audio window describes.
    [[nodiscard]] bool draw(StatusBar& status, const AudioEngine& engine);

    /// \return The Audio window, with what the user asked of it on the latest frame.
    [[nodiscard]] const AudioWindow& audio_window() const { return audio_window_; }

  private:
    bool instrument_open_ = true;
    bool audio_open_ = true;
    bool about_open_ = false;
    AudioWindow audio_window_;
};

} // namespace irama_studio
