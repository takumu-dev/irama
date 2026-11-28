// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

/// \file
/// A pattern part: a pattern's lines, and the note cells of every note column on each.

#pragma once

#include <irama/engine/base/allocator.hpp>
#include <irama/engine/base/owned_array.hpp>
#include <irama/engine/base/owned_ptr.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>

namespace irama {

/// One note column's cell on one line: five fields of one byte each, `empty` where nothing is
/// written.
struct NoteCell {
    /// A field with nothing written in it.
    static constexpr std::uint8_t empty = 0xFF;
    /// The note of an `OFF`.
    static constexpr std::uint8_t off = 0xFE;

    /// The note, from 0 for C-0 to 119 for B-9, or `off`, or `empty`.
    std::uint8_t note = empty;
    /// The instrument number, from 0x01 to 0xFE, or `empty`.
    std::uint8_t instrument = empty;
    /// The volume, from 0x00 to 0x80, or `empty`.
    std::uint8_t volume = empty;
    /// The pan, from 0x00 for hard left to 0x80 for hard right, or `empty`.
    std::uint8_t pan = empty;
    /// Where in the line the note starts, in 256ths of a line, or `empty`.
    std::uint8_t delay = empty;

    /// \return Whether two cells hold the same fields.
    friend bool operator==(const NoteCell&, const NoteCell&) = default;
};

class PatternPart;

/// The single owner of a pattern part.
using PatternPartPtr = OwnedPtr<PatternPart>;

/// The part of an EngineSong that holds one pattern: its line count, and the note cells of every
/// track's note columns, line by line.
///
/// A line holds one cell for each note column of the song, the first track's columns first, in the
/// order of the song part's tracks. A pattern part never changes once it is built.
class PatternPart {
    // Lets create() alone construct a part, through make_owned().
    struct Key {
        explicit Key() = default;
    };

  public:
    /// The most lines a pattern has.
    static constexpr std::size_t max_lines = 512;

    /// The most note columns on a line: 64 tracks of 12.
    static constexpr std::size_t max_columns = 768;

    /// Builds a pattern part, on a control thread.
    ///
    /// \param allocator The allocator to draw on, which the part is freed to.
    /// \param lines How many lines, from 1 to `max_lines`, which is checked.
    /// \param columns How many note columns on each line, from 1 to `max_columns`, which is
    ///                checked.
    /// \param cells The cells, line by line, `lines × columns` of them, each field in its range,
    ///              which is checked. They are copied.
    /// \return The part, or `AllocationError::out_of_memory` when the allocator has no memory.
    [[nodiscard]] static std::expected<PatternPartPtr, AllocationError>
    create(const Allocator& allocator, std::size_t lines, std::size_t columns,
           std::span<const NoteCell> cells);

    /// Makes the part, for `create()` alone.
    ///
    /// \param key Which only `create()` can make.
    /// \param lines How many lines.
    /// \param columns How many note columns on each line.
    /// \param cells The cells' storage, already filled.
    PatternPart(Key key, std::size_t lines, std::size_t columns, OwnedArray<NoteCell> cells);

    /// \return How many lines the pattern has.
    [[nodiscard]] std::size_t lines() const { return lines_; }

    /// \return How many note columns each line has.
    [[nodiscard]] std::size_t columns() const { return columns_; }

    /// \param line The line, less than `lines()`, which is checked where `NDEBUG` isn't defined.
    /// \return The line's cells, one for each note column.
    [[nodiscard]] std::span<const NoteCell> line(std::size_t line) const;

    /// \return Every cell, line by line.
    [[nodiscard]] std::span<const NoteCell> cells() const { return cells_.span(); }

    /// \return Whether two parts hold the same pattern.
    friend bool operator==(const PatternPart& left, const PatternPart& right);

  private:
    std::uint16_t lines_;
    std::uint16_t columns_;
    OwnedArray<NoteCell> cells_;
};

} // namespace irama
