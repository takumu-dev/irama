// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "input/shortcut_table.hpp"

namespace irama_studio {

class NoteKeys;

/// Runs the Studio function a shortcut names, whether its chord was pressed or its menu entry
/// chosen.
///
/// \param id The shortcut.
/// \param note_keys The note keys, whose keyboard octave the octave shortcuts step.
void run_shortcut(ShortcutId id, NoteKeys& note_keys);

} // namespace irama_studio
