// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "app/shortcut_actions.hpp"

#include "app/song_editor.hpp"
#include "input/note_keys.hpp"
#include "input/shortcut_table.hpp"

namespace irama_studio {

void run_shortcut(ShortcutId id, NoteKeys& note_keys, SongEditor& editor, AudioEngine& engine) {
    switch (id) {
    case ShortcutId::lower_octave:
        note_keys.set_octave(note_keys.octave() - 1);
        break;
    case ShortcutId::raise_octave:
        note_keys.set_octave(note_keys.octave() + 1);
        break;
    case ShortcutId::undo:
        // The edits Studio makes record no view, so there is none to show.
        static_cast<void>(editor.undo(engine));
        break;
    case ShortcutId::redo:
        static_cast<void>(editor.redo(engine));
        break;
    }
}

} // namespace irama_studio
