// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include "document_builder.hpp"

#include "doc/document.hpp"
#include "doc/edit_scope.hpp"

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace irama_studio::test {
namespace {

using Outcome = std::expected<void, Refusal>;

constexpr std::array<std::string_view, 12> note_names{
    "C-", "C#", "D-", "D#", "E-", "F-", "F#", "G-", "G#", "A-", "A#", "B-",
};
constexpr std::size_t notes_per_octave = 12;
constexpr int hex = 16;

[[nodiscard]] std::string_view trimmed(std::string_view text) {
    const std::size_t first = text.find_first_not_of(' ');
    if (first == std::string_view::npos) {
        return {};
    }
    return text.substr(first, text.find_last_not_of(' ') - first + 1);
}

[[nodiscard]] std::vector<std::string_view> split(std::string_view text, char separator) {
    std::vector<std::string_view> parts;
    std::size_t start = 0;
    while (true) {
        const std::size_t end = text.find(separator, start);
        parts.push_back(text.substr(start, end - start));
        if (end == std::string_view::npos) {
            return parts;
        }
        start = end + 1;
    }
}

// A field of two hex digits, or `..` for empty.
[[nodiscard]] std::optional<std::uint8_t> read_byte(std::string_view text) {
    if (text == "..") {
        return NoteCell::empty;
    }
    std::uint8_t value = 0;
    const char* const last = std::to_address(text.end());
    const auto [end, error] = std::from_chars(std::to_address(text.begin()), last, value, hex);
    if (text.size() != 2 || error != std::errc{} || end != last) {
        return std::nullopt;
    }
    return value;
}

[[nodiscard]] std::optional<std::uint8_t> read_note(std::string_view text) {
    if (text == "...") {
        return NoteCell::empty;
    }
    if (text == "OFF") {
        return NoteCell::off;
    }
    const auto index = static_cast<std::size_t>(std::ranges::distance(
        note_names.begin(), std::ranges::find(note_names, text.substr(0, 2))));
    if (text.size() != 3 || index == note_names.size() || text.at(2) < '0' || text.at(2) > '9') {
        return std::nullopt;
    }
    return static_cast<std::uint8_t>(
        (static_cast<std::size_t>(text.at(2) - '0') * notes_per_octave) + index);
}

[[nodiscard]] std::optional<NoteCell> read_note_cell(std::string_view text) {
    const std::vector<std::string_view> fields = split(text, ' ');
    if (fields.size() != 5) {
        return std::nullopt;
    }
    const std::optional<std::uint8_t> note = read_note(fields.at(0));
    const std::optional<std::uint8_t> instrument = read_byte(fields.at(1));
    const std::optional<std::uint8_t> volume = read_byte(fields.at(2));
    const std::optional<std::uint8_t> pan = read_byte(fields.at(3));
    const std::optional<std::uint8_t> delay = read_byte(fields.at(4));
    if (!note || !instrument || !volume || !pan || !delay) {
        return std::nullopt;
    }
    return NoteCell{
        .note = *note,
        .instrument = *instrument,
        .volume = *volume,
        .pan = *pan,
        .delay = *delay,
    };
}

[[nodiscard]] std::optional<EffectCell> read_effect_cell(std::string_view text) {
    const std::vector<std::string_view> fields = split(text, ' ');
    if (fields.size() != 2 || fields.at(0).size() != 2) {
        return std::nullopt;
    }
    const std::string_view command = fields.at(0);
    EffectCell cell;
    if (command != "..") {
        cell.command = {command.at(0), command.at(1)};
    }
    const std::optional<std::uint8_t> value = read_byte(fields.at(1));
    if (!value) {
        return std::nullopt;
    }
    if (fields.at(1) != "..") {
        cell.value = *value;
        cell.has_value = true;
    }
    return cell;
}

} // namespace

void read_line(std::string_view text, const Track& track, std::vector<NoteCell>& notes,
               std::vector<EffectCell>& effects) {
    INFO("the line " << text);
    const std::vector<std::string_view> columns = split(text, '|');
    REQUIRE(columns.size() == std::size_t{track.note_columns} + track.effect_columns);
    notes.clear();
    effects.clear();
    for (std::size_t column = 0; column < columns.size(); ++column) {
        const std::string_view cell = trimmed(columns.at(column));
        INFO("the cell " << cell);
        if (column < track.note_columns) {
            const std::optional<NoteCell> read = read_note_cell(cell);
            REQUIRE(read);
            notes.push_back(held(read));
        } else {
            const std::optional<EffectCell> read = read_effect_cell(cell);
            REQUIRE(read);
            effects.push_back(held(read));
        }
    }
}

DocumentBuilder& DocumentBuilder::tracks(std::vector<Track> tracks) {
    write([tracks = std::move(tracks)](EditScope& scope) -> Outcome {
        scope.tracks() = tracks;
        for (std::size_t number = 0; number < Document::pattern_count; ++number) {
            if (const std::optional<Pattern>& pattern = scope.document().pattern(number)) {
                const std::size_t lines = pattern->head.lines;
                scope.pattern(number) = empty_pattern(scope.document(), lines);
            }
        }
        return {};
    });
    return *this;
}

DocumentBuilder& DocumentBuilder::pattern(std::size_t number, std::size_t lines) {
    write([number, lines](EditScope& scope) -> Outcome {
        scope.pattern(number) = empty_pattern(scope.document(), lines);
        return {};
    });
    return *this;
}

DocumentBuilder& DocumentBuilder::cells(std::size_t pattern, std::size_t track,
                                        const std::vector<std::string_view>& lines) {
    REQUIRE(document_.pattern(pattern));
    const Pattern& written = held(document_.pattern(pattern));
    REQUIRE(track < document_.tracks().size());
    REQUIRE(lines.size() <= written.head.lines);
    const Track& shape = document_.tracks().at(track);
    std::vector<NoteCell> notes;
    std::vector<EffectCell> effects;
    TrackBlock block = written.blocks.at(track);
    for (std::size_t line = 0; line < lines.size(); ++line) {
        read_line(lines.at(line), shape, notes, effects);
        std::ranges::copy(notes, block.notes.begin() +
                                     static_cast<std::ptrdiff_t>(line * shape.note_columns));
        std::ranges::copy(effects, block.effects.begin() +
                                       static_cast<std::ptrdiff_t>(line * shape.effect_columns));
    }
    write([pattern, track, block = std::move(block)](EditScope& scope) -> Outcome {
        scope.block(pattern, track) = block;
        return {};
    });
    return *this;
}

DocumentBuilder& DocumentBuilder::sequence(std::vector<std::uint16_t> slots) {
    write([slots = std::move(slots)](EditScope& scope) -> Outcome {
        scope.sequence().slots = slots;
        return {};
    });
    return *this;
}

DocumentBuilder& DocumentBuilder::section(std::string name, std::size_t start_slot) {
    write([name = std::move(name), start_slot](EditScope& scope) -> Outcome {
        scope.sequence().sections.push_back({.name = name, .start_slot = start_slot});
        return {};
    });
    return *this;
}

DocumentBuilder& DocumentBuilder::parameter(SongParameter parameter) {
    write([parameter = std::move(parameter)](EditScope& scope) -> Outcome {
        scope.parameters().push_back(parameter);
        return {};
    });
    return *this;
}

DocumentBuilder& DocumentBuilder::marker(std::size_t pattern, std::size_t line, std::string name) {
    write([pattern, line, name = std::move(name)](EditScope& scope) -> Outcome {
        std::vector<Marker>& markers = scope.pattern_head(pattern).markers;
        markers.insert(std::ranges::upper_bound(markers, line, {}, &Marker::line),
                       {.line = line, .name = name});
        return {};
    });
    return *this;
}

DocumentBuilder& DocumentBuilder::instrument(std::size_t number, std::string name) {
    write([number, name = std::move(name)](EditScope& scope) -> Outcome {
        Instrument instrument = default_instrument();
        instrument.name = name;
        scope.instrument(number) = std::move(instrument);
        return {};
    });
    return *this;
}

DocumentBuilder& DocumentBuilder::metadata(Metadata metadata) {
    write([metadata = std::move(metadata)](EditScope& scope) -> Outcome {
        scope.info().metadata = metadata;
        return {};
    });
    return *this;
}

void DocumentBuilder::write(const EditFunction& function) {
    const std::expected<EditRun, Refusal> run = EditRun::run(document_, function);
    INFO("the Document refused: " << (run ? std::string{} : run.error()));
    REQUIRE(run);
}

Document test_song() {
    return DocumentBuilder{}
        .tracks({
            {.name = "Lead", .note_columns = 2, .effect_columns = 1},
            {.name = "Bass", .note_columns = 1, .effect_columns = 0},
        })
        .pattern(0, 8)
        .pattern(1, 4)
        .pattern(7, 16)
        .cells(0, 0,
               {
                   "C-4 01 40 .. .. | ... .. .. .. .. | 0A 40",
                   "... .. .. .. .. | E-4 02 .. 40 80 | .. ..",
                   "OFF .. .. .. 10 | OFF .. .. .. .. | ZZ ..",
               })
        .cells(0, 1, {"C-2 03 80 .. ..", "... .. .. .. ..", "G#1 03 .. 00 .."})
        .cells(1, 1, {"A-3 01 .. .. ..", "OFF .. .. .. .."})
        .sequence({0, 1, 0, 7})
        .section("Intro", 0)
        .section("Verse", 2)
        .parameter({.name = "Intensity", .min = 0.0F, .max = 1.0F, .default_value = 0.5F})
        .marker(0, 0, "Drop")
        .marker(7, 12, "Fill")
        .instrument(2, "Pad")
        .instrument(3, "Bass")
        .metadata({.title = "Test song", .author = "Tests", .comments = "Built in code"})
        .build();
}

} // namespace irama_studio::test
