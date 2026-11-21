// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

namespace irama_studio {

/// Everything Studio's main window shows: the main menu bar, the dock space that fills the window
/// with the windows docked in it, the status bar, and the About window.
class Workspace {
  public:
    /// Draws one frame of the workspace, and returns false once the user has chosen to quit.
    [[nodiscard]] bool draw();

  private:
    bool instrument_open_ = true;
    bool audio_open_ = true;
    bool about_open_ = false;
};

} // namespace irama_studio
