// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

namespace irama_studio {

/// Draws the About window while `open`, which its close button clears: Studio's name, release
/// version and licence, the repository, and where the third-party notices are.
void draw_about(bool& open);

} // namespace irama_studio
