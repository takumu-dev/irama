// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include <irama/test_songs/test_songs.hpp>

#include <irama/engine/base/allocator.hpp>
#include <irama/engine/model/change_set.hpp>
#include <irama/engine/model/instrument_part.hpp>
#include <irama/engine/model/pattern_part.hpp>
#include <irama/engine/model/song_part.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <numeric>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace irama_test_songs {
namespace {

using irama::NoteCell;

constexpr std::size_t note_width = 3;
constexpr std::size_t field_width = 2;
constexpr std::size_t field_count = 4;
constexpr int semitones = 12;
constexpr std::uint8_t last_instrument = 0xFE;
constexpr std::uint8_t loudest = 0x80;
constexpr std::uint8_t hard_right = 0x80;
// A delay of FF would be the empty field's byte.
constexpr std::uint8_t last_delay = 0xFE;

// The new song's tracks, which a song part has when its description leaves them empty.
constexpr std::array<std::uint8_t, 4> new_song_note_columns{1, 1, 1, 1};

[[nodiscard]] std::unexpected<std::string> fault(std::string_view text, std::string_view what) {
    return std::unexpected(std::string{"\""}.append(text).append("\": ").append(what));
}

[[nodiscard]] std::optional<int> hex_digit(char digit) {
    if (digit >= '0' && digit <= '9') {
        return digit - '0';
    }
    if (digit >= 'A' && digit <= 'F') {
        return digit - 'A' + 10;
    }
    if (digit >= 'a' && digit <= 'f') {
        return digit - 'a' + 10;
    }
    return std::nullopt;
}

// A note's three characters: a letter, `-` or `#`, and an octave.
[[nodiscard]] std::optional<std::uint8_t> parse_note(std::string_view note) {
    if (note == "OFF") {
        return NoteCell::off;
    }
    if (note == "...") {
        return NoteCell::empty;
    }
    // The semitone of each letter from A, counting from C.
    constexpr std::array<int, 7> letters{9, 11, 0, 2, 4, 5, 7};
    const char letter = note.at(0);
    const char accidental = note.at(1);
    const char octave = note.at(2);
    if (letter < 'A' || letter > 'G' || octave < '0' || octave > '9') {
        return std::nullopt;
    }
    const int natural = letters.at(static_cast<std::size_t>(letter - 'A'));
    const bool sharp = accidental == '#';
    if ((!sharp && accidental != '-') || (sharp && (natural == 4 || natural == 11))) {
        return std::nullopt;
    }
    return static_cast<std::uint8_t>(((octave - '0') * semitones) + natural + (sharp ? 1 : 0));
}

// Two hex digits.
[[nodiscard]] std::optional<std::uint8_t> parse_hex(std::string_view field) {
    const std::optional<int> high = hex_digit(field.at(0));
    const std::optional<int> low = hex_digit(field.at(1));
    if (!high || !low) {
        return std::nullopt;
    }
    return static_cast<std::uint8_t>((*high * 16) + *low);
}

} // namespace

std::expected<NoteCell, std::string> parse_cell(std::string_view text) {
    NoteCell cell;
    if (text.empty()) {
        return cell;
    }
    if (text.size() < note_width) {
        return fault(text, "a cell starts with a note of three characters");
    }
    const std::optional<std::uint8_t> note = parse_note(text.substr(0, note_width));
    if (!note) {
        return fault(text, "a note is written as C-4, C#4, OFF or ...");
    }
    cell.note = *note;

    // Each field after the note, the values it can hold, and its range in words.
    struct Field {
        std::uint8_t* value;
        std::uint8_t first;
        std::uint8_t last;
        std::string_view range;
    };

    const std::array<Field, field_count> fields{
        Field{
            .value = &cell.instrument,
            .first = 1,
            .last = last_instrument,
            .range = "an instrument number is from 01 to FE",
        },
        Field{
            .value = &cell.volume,
            .first = 0,
            .last = loudest,
            .range = "a volume is from 00 to 80",
        },
        Field{
            .value = &cell.pan,
            .first = 0,
            .last = hard_right,
            .range = "a pan is from 00 to 80",
        },
        Field{
            .value = &cell.delay,
            .first = 0,
            .last = last_delay,
            .range = "a delay is from 00 to FE",
        },
    };
    std::string_view rest = text.substr(note_width);
    for (const Field& field : fields) {
        if (rest.empty()) {
            break;
        }
        if (rest.size() < field_width + 1 || rest.front() != ' ') {
            return fault(text, "each field after the note is a space and two characters");
        }
        const std::string_view written = rest.substr(1, field_width);
        rest = rest.substr(field_width + 1);
        if (written == "..") {
            continue;
        }
        const std::optional<std::uint8_t> value = parse_hex(written);
        if (!value) {
            return fault(text, "a field is two hex digits or ..");
        }
        if (*value < field.first || *value > field.last) {
            return fault(text, field.range);
        }
        *field.value = *value;
    }
    if (!rest.empty()) {
        return fault(text, "a cell has at most five fields");
    }
    return cell;
}

std::expected<irama::ChangeSetPtr, std::string> song_change(const SongText& song,
                                                            const irama::Allocator& allocator) {
    const std::span<const std::uint8_t> note_columns = song.description.note_columns.empty()
                                                           ? std::span{new_song_note_columns}
                                                           : song.description.note_columns;
    const std::size_t columns =
        std::accumulate(note_columns.begin(), note_columns.end(), std::size_t{0});
    auto changes = irama::ChangeSet::create(
        allocator, {.patterns = song.patterns.size(), .instruments = song.instruments.size()});
    auto part = irama::SongPart::create(allocator, song.description);
    if (!changes || !part) {
        return std::unexpected("out of memory");
    }
    (*changes)->replace_song(std::move(*part));
    for (const PatternText& pattern : song.patterns) {
        const std::size_t lines = std::max(pattern.length, pattern.lines.size());
        if (lines < 1 || lines > irama::PatternPart::max_lines) {
            return std::unexpected("a pattern has from 1 to 512 lines");
        }
        std::vector<NoteCell> cells(lines * columns);
        for (std::size_t line = 0; line < pattern.lines.size(); ++line) {
            const Line& texts = pattern.lines.at(line);
            if (texts.size() > columns) {
                return std::unexpected("a line has more cells than the song has note columns");
            }
            for (std::size_t column = 0; column < texts.size(); ++column) {
                auto cell = parse_cell(texts.at(column));
                if (!cell) {
                    return std::unexpected(std::move(cell.error()));
                }
                cells.at((line * columns) + column) = *cell;
            }
        }
        auto built = irama::PatternPart::create(allocator, lines, columns, cells);
        if (!built) {
            return std::unexpected("out of memory");
        }
        (*changes)->replace_pattern(pattern.number, std::move(*built));
    }
    for (const InstrumentValues& instrument : song.instruments) {
        auto built = irama::InstrumentPart::create(allocator, instrument.values);
        if (!built) {
            return std::unexpected("out of memory");
        }
        (*changes)->replace_instrument(instrument.number, std::move(*built));
    }
    return std::move(*changes);
}

} // namespace irama_test_songs
