// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#pragma once

#include "doc/document.hpp"
#include "doc/edit_scope.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace irama_studio::test {

/// What an optional holds, for a test that has checked it holds something. An empty one throws,
/// which fails the test.
template <typename T> [[nodiscard]] const T& held(const std::optional<T>& value) {
    if (!value) {
        throw std::logic_error{"the optional is empty"};
    }
    return *value;
}

/// What a returned optional holds, as `held` above.
template <typename T> [[nodiscard]] T held(std::optional<T>&& value) {
    std::optional<T> taken = std::move(value);
    if (!taken) {
        throw std::logic_error{"the optional is empty"};
    }
    return std::move(*taken);
}

/// Builds test songs from the new song, step by step, each step an edit run on the Document. A step
/// that the Document refuses fails the test.
///
/// Cells are written as the pattern grid shows them, one string for each line of a track: each
/// note column as `C-4 01 40 .. 00` (note, instrument, volume, pan, delay), each effect column as
/// `0A 40` (command, value), the columns parted by `|`. An empty note is `...`, an `OFF` is `OFF`,
/// and any other empty field is `..`. A malformed cell fails the test.
class DocumentBuilder {
  public:
    /// Sets the track list. Every pattern already made starts again with empty cells, so tracks
    /// come first.
    DocumentBuilder& tracks(std::vector<Track> tracks);

    /// Makes an empty pattern, or empties one, with `lines` lines.
    DocumentBuilder& pattern(std::size_t number, std::size_t lines);

    /// Writes a track's cells in a pattern from line 0, one string for each line.
    DocumentBuilder& cells(std::size_t pattern, std::size_t track,
                           const std::vector<std::string_view>& lines);

    /// Sets the sequence's slots.
    DocumentBuilder& sequence(std::vector<std::uint16_t> slots);

    /// Adds a section starting on a slot after the last section's.
    DocumentBuilder& section(std::string name, std::size_t start_slot);

    /// Adds a song parameter.
    DocumentBuilder& parameter(SongParameter parameter);

    /// Adds a marker to a pattern.
    DocumentBuilder& marker(std::size_t pattern, std::size_t line, std::string name);

    /// Puts an instrument with every default and a name at an instrument number.
    DocumentBuilder& instrument(std::size_t number, std::string name);

    /// Sets the metadata.
    DocumentBuilder& metadata(Metadata metadata);

    /// \return The song as built.
    [[nodiscard]] const Document& build() const { return document_; }

  private:
    // Runs one step, which must not be refused.
    void write(const EditFunction& function);

    Document document_;
};

/// Reads one line of a track's cells, written as the grid shows them, into the track's note and
/// effect columns. A malformed line fails the test.
///
/// \param text The line.
/// \param track The track, whose column counts the line must match.
/// \param notes Where the note cells go, one for each note column.
/// \param effects Where the effect cells go, one for each effect column.
void read_line(std::string_view text, const Track& track, std::vector<NoteCell>& notes,
               std::vector<EffectCell>& effects);

/// A test song of 3 patterns over 4 slots, 2 tracks of different shapes, 3 instruments, markers,
/// sections and song parameters, with notes in it.
[[nodiscard]] Document test_song();

} // namespace irama_studio::test
