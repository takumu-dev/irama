// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include <irama/test_songs/test_songs.hpp>

#include <irama/engine/model/change_set.hpp>
#include <irama/engine/model/engine_song.hpp>
#include <irama/engine/model/pattern_part.hpp>
#include <irama/engine/model/song_part.hpp>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <string_view>

using irama::NoteCell;
using irama_test_songs::parse_cell;

TEST_CASE("a cell is read as the grid shows it", "[test songs]") {
    CHECK(parse_cell("C-4 01 40 .. 00") ==
          NoteCell{.note = 48, .instrument = 0x01, .volume = 0x40, .delay = 0x00});
    CHECK(parse_cell("OFF .. .. .. 10") == NoteCell{.note = NoteCell::off, .delay = 0x10});
    CHECK(parse_cell("F#3 fe 80 00 fe") ==
          NoteCell{.note = 42, .instrument = 0xFE, .volume = 0x80, .pan = 0x00, .delay = 0xFE});
    CHECK(parse_cell("B-9") == NoteCell{.note = 119});
    CHECK(parse_cell("C-0 02") == NoteCell{.note = 0, .instrument = 0x02});
    CHECK(parse_cell("... .. .. 40") == NoteCell{.pan = 0x40});
    CHECK(parse_cell("") == NoteCell{});
}

TEST_CASE("a malformed cell is refused, saying why", "[test songs]") {
    const std::string_view text = GENERATE(
        "C4", "H-4", "E#4", "B#4", "C-A", "C_4", "C-4 1", "C-4  01", "C-4 0g", "C-4 00", "C-4 FF",
        "C-4 01 81", "C-4 01 40 81", "C-4 01 40 40 00 00", "C-4 01 40 40 FF", "OFF,..");
    CAPTURE(text);
    const auto cell = parse_cell(text);
    REQUIRE_FALSE(cell.has_value());
    CHECK(cell.error().find(text) != std::string_view::npos);
}

TEST_CASE("a song's change set holds its song part, patterns and instruments", "[test songs]") {
    constexpr std::array<std::uint8_t, 2> note_columns{2, 1};
    constexpr std::array<std::uint16_t, 2> sequence{3, 3};
    auto changes = irama_test_songs::song_change({
        .description = {.bpm100 = 12'345, .note_columns = note_columns, .sequence = sequence},
        .patterns = {{.number = 3, .lines = {{"C-4 01", "", "OFF"}, {}, {"", "D-4"}}, .length = 4}},
        .instruments = {{.number = 0x02, .values = {1.0F, 2.0F}}},
    });
    REQUIRE(changes.has_value());
    irama::EngineSong song;
    song.apply(**changes);

    REQUIRE(song.song() != nullptr);
    CHECK(song.song()->bpm100() == 12'345);
    const irama::PatternPart* pattern = song.pattern(3);
    REQUIRE(pattern != nullptr);
    CHECK(pattern->lines() == 4);
    CHECK(pattern->columns() == 3);
    CHECK(std::ranges::equal(pattern->line(0),
                             std::array{NoteCell{.note = 48, .instrument = 0x01}, NoteCell{},
                                        NoteCell{.note = NoteCell::off}}));
    CHECK(std::ranges::equal(pattern->line(2),
                             std::array{NoteCell{}, NoteCell{.note = 50}, NoteCell{}}));
    CHECK(std::ranges::equal(pattern->line(3), std::array<NoteCell, 3>{}));
    CHECK(song.pattern(0) == nullptr);
    REQUIRE(song.instrument(0x02) != nullptr);
    CHECK(song.instrument(0x02)->values().size() == 2);
    CHECK(song.instrument(0x01) == nullptr);
}

TEST_CASE("a song with a malformed cell or a line too wide isn't built", "[test songs]") {
    CHECK_FALSE(irama_test_songs::song_change({.patterns = {{.lines = {{"C-4 0"}}}}}).has_value());
    CHECK_FALSE(irama_test_songs::song_change({.patterns = {{.lines = {{"", "", "", "", "C-4"}}}}})
                    .has_value());
    CHECK_FALSE(irama_test_songs::song_change({.patterns = {{}}}).has_value());
}
