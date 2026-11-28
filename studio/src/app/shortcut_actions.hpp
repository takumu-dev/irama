// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "input/shortcut_table.hpp"

namespace irama_studio {

class AudioEngine;
class NoteKeys;
class SongEditor;

/// Runs the Studio function a shortcut names, whether its chord was pressed or its menu entry
/// chosen.
///
/// \param id The shortcut.
/// \param note_keys The note keys, whose keyboard octave the octave shortcuts step.
/// \param editor The song, which undo and redo step through.
/// \param engine The audio engine, which undo and redo send the parts they change.
void run_shortcut(ShortcutId id, NoteKeys& note_keys, SongEditor& editor, AudioEngine& engine);

} // namespace irama_studio
