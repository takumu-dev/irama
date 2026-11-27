// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

// The scripts that `irama-cli synth` plays: one control message on each line, at an absolute frame.

#pragma once

#include <irama/engine/param/param_id.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <vector>

namespace irama_cli {

// One line of a script, checked, so that the player can take it as it is.
struct ScriptMessage {
    enum class Kind : std::uint8_t { note_on, note_off, release_all, set_parameter };

    std::uint64_t frame = 0;
    Kind kind = Kind::note_on;
    // The note of a note-on or a note-off, from 0 for C-0 to 119 for B-9.
    int note = 0;
    // The velocity of a note-on, from 0 to 1.
    float velocity = 0.0F;
    // The parameter of a set-parameter, one of the instrument's.
    irama::ParamId param{};
    // The value of a set-parameter, a finite number, which the player clamps to the parameter.
    float value = 0.0F;
};

// What is wrong with a script, and on which line, counting from 1.
struct ScriptError {
    std::size_t line = 0;
    std::string reason;
};

// Reads a script, a line for each message:
//
//     # a comment, alone or after a command
//     at 0      note-on C-4 0.8
//     at 12000  set-parameter 2001 -0.5
//     at 24000  note-off C-4
//     at 36000  release-all
//
// Notes are written as the note column writes them, from C-0 to B-9 with `#` for a sharp, and a
// velocity from 0 to 1. A parameter is named by its ID, and its value may be any finite number,
// which the player clamps to the parameter's range. Frames never go backwards. Every value is
// checked here, so that no script can break a precondition of the player.
[[nodiscard]] std::expected<std::vector<ScriptMessage>, ScriptError>
parse_script(std::string_view text);

} // namespace irama_cli
