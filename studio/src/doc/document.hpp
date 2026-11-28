// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <irama/engine/model/pattern_part.hpp>
#include <irama/engine/param/param_descriptor.hpp>
#include <irama/engine/param/param_id.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace irama_studio {

/// One note column's cell on one line: note, instrument, volume, pan and delay, one byte each, in
/// the engine's own encoding, with `0xFF` for an empty field.
using irama::NoteCell;

/// One effect column's cell on one line: a command of two characters and a value byte, each of
/// them or both empty. The player doesn't read effect columns, so they play nothing.
struct EffectCell {
    /// The command's two characters, each `0`–`9` or `A`–`Z`, or both `'\0'` for no command.
    std::array<char, 2> command{};
    /// The value, meaningful only while `has_value`, since every byte is a valid value.
    std::uint8_t value = 0;
    bool has_value = false;

    /// \return Whether the cell holds neither a command nor a value.
    [[nodiscard]] bool empty() const { return command == std::array<char, 2>{} && !has_value; }

    friend bool operator==(const EffectCell&, const EffectCell&) = default;
};

/// The song's title, author and comments.
struct Metadata {
    std::string title{};
    std::string author{};
    std::string comments{};

    friend bool operator==(const Metadata&, const Metadata&) = default;
};

/// The song's timing, which holds for the whole song.
struct Timing {
    /// Beats per minute, from 20 to 999, edited to 2 decimals.
    double bpm = 120.0;
    /// Lines per beat, from 1 to 64.
    std::uint8_t lpb = 4;
    /// Beats per bar, from 1 to 16.
    std::uint8_t beats_per_bar = 4;
    /// Ticks per line, from 1 to 32.
    std::uint8_t ticks_per_line = 12;

    friend bool operator==(const Timing&, const Timing&) = default;
};

/// What belongs to the whole song and nothing else: its metadata, its timing and its seed.
struct SongInfo {
    Metadata metadata{};
    Timing timing{};
    /// The seed of every random source.
    std::uint64_t seed = 0x6972'616d'6131;

    friend bool operator==(const SongInfo&, const SongInfo&) = default;
};

/// A sequencer track: a name, which may be empty or repeat, and its column counts, which every
/// pattern shares.
struct Track {
    std::string name{};
    /// From 1 to 12.
    std::uint8_t note_columns = 1;
    /// From 0 to 8.
    std::uint8_t effect_columns = 1;

    friend bool operator==(const Track&, const Track&) = default;
};

/// A section: a name, and the slot it starts on. It runs to the next section's start or the end
/// of the sequence.
struct Section {
    /// Non-empty, unique among sections, with no space at either end.
    std::string name{};
    std::size_t start_slot = 0;

    friend bool operator==(const Section&, const Section&) = default;
};

/// The song's order: its slots, each holding a pattern number, and the sections over them.
struct Sequence {
    /// The pattern number of each slot, from 1 to 1,000 slots.
    std::vector<std::uint16_t> slots{};
    /// The sections, in the order of their start slots, at most one starting on each slot.
    std::vector<Section> sections{};

    friend bool operator==(const Sequence&, const Sequence&) = default;
};

/// A song parameter: a name and a range with a default.
struct SongParameter {
    /// Non-empty, unique among song parameters, with no space at either end.
    std::string name{};
    float min = 0.0F;
    float max = 1.0F;
    /// From `min` to `max`.
    float default_value = 0.0F;

    friend bool operator==(const SongParameter&, const SongParameter&) = default;
};

/// A marker: a named line in a pattern.
struct Marker {
    std::size_t line = 0;
    /// Non-empty, and may repeat.
    std::string name{};

    friend bool operator==(const Marker&, const Marker&) = default;
};

/// One pattern's cells in one track, line by line: the track's note columns of each line, then
/// its effect columns of each line.
struct TrackBlock {
    /// The pattern's line count times the track's note columns.
    std::vector<NoteCell> notes{};
    /// The pattern's line count times the track's effect columns.
    std::vector<EffectCell> effects{};

    friend bool operator==(const TrackBlock&, const TrackBlock&) = default;
};

/// What a pattern holds beside its cells: its line count and its markers.
struct PatternHead {
    /// From 1 to 512.
    std::size_t lines = 64;
    /// In the order of their lines, each on a line of the pattern, at most one on each line.
    std::vector<Marker> markers{};

    friend bool operator==(const PatternHead&, const PatternHead&) = default;
};

/// A pattern: its line count and markers, and one block of cells for each track.
struct Pattern {
    PatternHead head{};
    /// One for each track, in the order of the tracks.
    std::vector<TrackBlock> blocks{};

    friend bool operator==(const Pattern&, const Pattern&) = default;
};

/// An instrument: a name, which may be empty or repeat, and one plain value for each parameter in
/// the basic synth's table, in the order of the table.
struct Instrument {
    std::string name{};
    /// Each in its parameter's range, and whole where the parameter is stepped.
    std::vector<float> values{};

    /// \param id The ID of one of the table's parameters, which is checked.
    /// \return The parameter's value.
    [[nodiscard]] float value(irama::ParamId id) const;

    friend bool operator==(const Instrument&, const Instrument&) = default;
};

/// The parameters every instrument holds a value for.
[[nodiscard]] std::span<const irama::ParamDescriptor> instrument_table();

/// \return An instrument with an empty name and every parameter at its default.
[[nodiscard]] Instrument default_instrument();

/// The song Studio edits: everything an edit changes, undo restores and the EngineSong is built
/// from. It holds plain values, and every reference in it is a number or a position, never a
/// pointer.
///
/// Patterns and instruments are known by their numbers, which never change, and tracks and slots by
/// their positions. Reading is open to all, and writing only to an edit, through its `EditScope`.
/// Every limit below holds between edits.
class Document {
  public:
    static constexpr std::size_t max_tracks = 64;
    static constexpr std::size_t max_note_columns = 12;
    static constexpr std::size_t max_effect_columns = 8;
    static constexpr std::size_t pattern_count = 1'000;
    static constexpr std::size_t max_lines = 512;
    static constexpr std::size_t max_slots = 1'000;
    static constexpr std::size_t max_parameters = 64;
    static constexpr std::size_t first_instrument = 0x01;
    static constexpr std::size_t last_instrument = 0xFE;
    static constexpr std::size_t instrument_count = last_instrument - first_instrument + 1;
    static constexpr double min_bpm = 20.0;
    static constexpr double max_bpm = 999.0;

    /// Makes the new song: no metadata, the default timing and seed, 4 tracks of one note column
    /// and one effect column, pattern 000 of 64 empty lines in one slot, and instrument 01 with
    /// every default.
    Document();

    [[nodiscard]] const SongInfo& info() const { return info_; }

    [[nodiscard]] const std::vector<Track>& tracks() const { return tracks_; }

    [[nodiscard]] const Sequence& sequence() const { return sequence_; }

    [[nodiscard]] const std::vector<SongParameter>& parameters() const { return parameters_; }

    /// \param number The pattern number, from 0 to 999, which is checked.
    /// \return The pattern, or nothing for an empty number.
    [[nodiscard]] const std::optional<Pattern>& pattern(std::size_t number) const;

    /// \param number The instrument number, from 0x01 to 0xFE, which is checked.
    /// \return The instrument, or nothing for an empty number.
    [[nodiscard]] const std::optional<Instrument>& instrument(std::size_t number) const;

    friend bool operator==(const Document&, const Document&) = default;

  private:
    // The one way to write the Document, and the copies that undo and redo swap back in.
    friend class EditScope;
    friend class PartCopies;

    SongInfo info_;
    std::vector<Track> tracks_;
    Sequence sequence_;
    std::vector<SongParameter> parameters_;
    std::vector<std::optional<Pattern>> patterns_;
    std::vector<std::optional<Instrument>> instruments_;
};

/// \return An empty pattern of `lines` lines, shaped for the document's tracks.
[[nodiscard]] Pattern empty_pattern(const Document& document, std::size_t lines);

/// \return The lowest pattern number that holds no pattern, if any.
[[nodiscard]] std::optional<std::size_t> lowest_free_pattern(const Document& document);

/// \return The lowest instrument number that holds no instrument, if any.
[[nodiscard]] std::optional<std::size_t> lowest_free_instrument(const Document& document);

} // namespace irama_studio
