// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#pragma once

#include "audio/sent_song.hpp"
#include "doc/changed_parts.hpp"
#include "doc/document.hpp"
#include "doc/edit_scope.hpp"

#include <catch2/catch_tostring.hpp>

#include <string>

namespace irama_studio::test {

/// Checks an edit as every kind of edit is checked. It applies the edit to `start` through an undo
/// history, then undoes and redoes it, and after each step checks:
///
/// - the Document: changed by the edit, equal to `start` after the undo, and equal to the edited
///   Document again after the redo;
/// - the parts reported: `reported` for the edit and the redo, and the same parts with the position
///   map turned around for the undo;
/// - incremental equals whole: the song as sent, updated by the parts reported, equals the song
///   built from scratch from the Document.
///
/// \return The Document after the edit.
Document check_edit(const Document& start, const Edit& edit, const ChangedParts& reported);

/// Brings the song as sent up to date with what a change reported, and checks that it then equals
/// the song built from scratch from the Document.
void check_incremental_equals_whole(SentSong& sent, const Document& document,
                                    const ChangedParts& parts);

} // namespace irama_studio::test

template <> struct Catch::StringMaker<irama_studio::ChangedParts> {
    static std::string convert(const irama_studio::ChangedParts& parts);
};
