// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

/// \file
/// Test songs written as the pattern grid shows them, built into the change set that loads them.

#pragma once

#include <irama/engine/base/allocator.hpp>
#include <irama/engine/model/change_set.hpp>
#include <irama/engine/model/pattern_part.hpp>
#include <irama/engine/model/song_part.hpp>

#include <cstddef>
#include <expected>
#include <string>
#include <string_view>
#include <vector>

namespace irama_test_songs {

/// One line of a pattern: a cell's text for each note column, the first track's first. Columns past
/// the last cell given are empty.
using Line = std::vector<std::string_view>;

/// A pattern as the grid shows it.
struct PatternText {
    /// The pattern number, from 0 to 999.
    std::size_t number = 0;
    /// Its lines, from the first.
    std::vector<Line> lines{};
    /// How many lines it has, at least as many as `lines`, or 0 for exactly as many.
    std::size_t length = 0;
};

/// An instrument number and its plain values, one for each parameter.
struct InstrumentValues {
    /// The instrument number, from 0x01 to 0xFE.
    std::size_t number = 0;
    /// Its values.
    std::vector<float> values{};
};

/// A whole song: its song part's description, its patterns and its instruments. Every other number
/// is empty.
struct SongText {
    /// The timing, the seed, the tracks and the sequence, as a song part is built from.
    irama::SongDescription description{};
    /// The patterns.
    std::vector<PatternText> patterns{};
    /// The instruments.
    std::vector<InstrumentValues> instruments{};
};

/// Reads a note cell written as the grid shows it, with dots for an empty field: a note such as
/// `C-4` or `F#3`, `OFF` or `...`, then the instrument, the volume, the pan and the delay as two
/// hex digits each, or `..`, each after a space, a delay up to FE since FF is the empty byte, as in
/// `"C-4 01 40 .. 00"`. Fields left off the end are empty, so `"C-4 01"` is a note on instrument
/// 01, and `""` is an empty cell.
///
/// \param text The cell.
/// \return The cell, or what is wrong with the text.
[[nodiscard]] std::expected<irama::NoteCell, std::string> parse_cell(std::string_view text);

/// Builds every part of a song into one change set, which loads it into a player or an EngineSong.
///
/// \param song The song. The description's fields are in their ranges, which `irama::SongPart`
///             checks.
/// \param allocator The allocator to draw on.
/// \return The change set, or what is wrong with the song.
[[nodiscard]] std::expected<irama::ChangeSetPtr, std::string>
song_change(const SongText& song, const irama::Allocator& allocator = irama::default_allocator());

} // namespace irama_test_songs
