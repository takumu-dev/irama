// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "views/instrument_window.hpp"

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
    /// \param engine The audio engine, which the Audio window describes and the Instrument window
    ///               changes the instrument's values through.
    [[nodiscard]] bool draw(StatusBar& status, AudioEngine& engine);

  private:
    InstrumentWindow instrument_;
    bool instrument_open_ = true;
    bool audio_open_ = true;
    bool about_open_ = false;
};

} // namespace irama_studio
