// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "app/shortcut_actions.hpp"

#include "input/note_keys.hpp"
#include "input/shortcut_table.hpp"

namespace irama_studio {

void run_shortcut(ShortcutId id, NoteKeys& note_keys) {
    switch (id) {
    case ShortcutId::lower_octave:
        note_keys.set_octave(note_keys.octave() - 1);
        break;
    case ShortcutId::raise_octave:
        note_keys.set_octave(note_keys.octave() + 1);
        break;
    }
}

} // namespace irama_studio
