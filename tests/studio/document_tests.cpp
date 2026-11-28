// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include "doc/document.hpp"

#include "document_builder.hpp"
#include <irama/engine/model/pattern_part.hpp>
#include <irama/engine/param/param_descriptor.hpp>
#include <irama/engine/voice/instrument.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <ranges>
#include <vector>

using irama_studio::Document;
using irama_studio::EffectCell;
using irama_studio::NoteCell;
using irama_studio::Track;
using irama_studio::test::held;

TEST_CASE("the new song has its default info, 4 tracks and pattern 000 in one slot",
          "[studio][document]") {
    const Document song;
    CHECK(song.info().metadata.title.empty());
    CHECK(song.info().metadata.author.empty());
    CHECK(song.info().metadata.comments.empty());
    CHECK(song.info().timing.bpm == 120.0);
    CHECK(song.info().timing.lpb == 4);
    CHECK(song.info().timing.beats_per_bar == 4);
    CHECK(song.info().timing.ticks_per_line == 12);
    CHECK(song.info().seed == 0x6972'616d'6131);

    REQUIRE(song.tracks().size() == 4);
    CHECK(song.tracks().front() == Track{.name = "Track 1"});
    CHECK(song.tracks().back() == Track{.name = "Track 4"});

    CHECK(song.sequence().slots == std::vector<std::uint16_t>{0});
    CHECK(song.sequence().sections.empty());
    CHECK(song.parameters().empty());
}

TEST_CASE("the new song's pattern 000 has 64 empty lines and is its only pattern",
          "[studio][document]") {
    const Document song;
    REQUIRE(song.pattern(0));
    const irama_studio::Pattern& pattern = held(song.pattern(0));
    CHECK(pattern.head.lines == 64);
    CHECK(pattern.head.markers.empty());
    REQUIRE(pattern.blocks.size() == 4);
    CHECK(pattern.blocks.front().notes == std::vector<NoteCell>(64));
    CHECK(pattern.blocks.front().effects == std::vector<EffectCell>(64));
    CHECK(std::ranges::none_of(
        std::views::iota(std::size_t{1}, Document::pattern_count),
        [&song](std::size_t number) { return song.pattern(number).has_value(); }));
}

TEST_CASE("the new song's instrument 01 is unnamed, at every default, and its only instrument",
          "[studio][document]") {
    const Document song;
    REQUIRE(song.instrument(0x01));
    const irama_studio::Instrument& instrument = held(song.instrument(0x01));
    CHECK(instrument.name.empty());
    for (const irama::ParamDescriptor& param : irama::instrument_params) {
        CHECK(instrument.value(param.id) == param.default_value);
    }
    CHECK(std::ranges::none_of(
        std::views::iota(std::size_t{0x02}, std::size_t{0xFF}),
        [&song](std::size_t number) { return song.instrument(number).has_value(); }));
}

TEST_CASE("Documents compare equal field by field", "[studio][document]") {
    CHECK(Document{} == Document{});
    CHECK(irama_studio::test::test_song() == irama_studio::test::test_song());
    CHECK(irama_studio::test::test_song() != Document{});
}

TEST_CASE("the lowest free numbers skip the patterns and instruments that are filled",
          "[studio][document]") {
    const Document song = irama_studio::test::test_song();
    CHECK(irama_studio::lowest_free_pattern(song) == 2);
    CHECK(irama_studio::lowest_free_instrument(song) == 4);
    CHECK(irama_studio::lowest_free_pattern(Document{}) == 1);
    CHECK(irama_studio::lowest_free_instrument(Document{}) == 2);
}

TEST_CASE("the Document builder reads cells written as the grid shows them", "[studio][document]") {
    const Document song = irama_studio::test::test_song();
    const irama_studio::Pattern& pattern = held(song.pattern(0));
    const irama_studio::TrackBlock& lead = pattern.blocks.at(0);
    // Line 0: C-4 on instrument 01 at volume 40 in the first column, and an empty second column.
    CHECK(lead.notes.at(0) == NoteCell{.note = 48, .instrument = 0x01, .volume = 0x40});
    CHECK(lead.notes.at(1) == NoteCell{});
    CHECK(lead.effects.at(0) ==
          EffectCell{.command = {'0', 'A'}, .value = 0x40, .has_value = true});
    // Line 1: E-4 in the second column, panned centre with a delay of 80.
    CHECK(lead.notes.at(3) == NoteCell{.note = 52, .instrument = 0x02, .pan = 0x40, .delay = 0x80});
    CHECK(lead.effects.at(1) == EffectCell{});
    // Line 2: two OFFs, the first delayed, and a command with no value.
    CHECK(lead.notes.at(4) == NoteCell{.note = NoteCell::off, .delay = 0x10});
    CHECK(lead.notes.at(5) == NoteCell{.note = NoteCell::off});
    CHECK(lead.effects.at(2) == EffectCell{.command = {'Z', 'Z'}});
    // The lines not written stay empty.
    CHECK(lead.notes.at(6) == NoteCell{});

    const irama_studio::TrackBlock& bass = pattern.blocks.at(1);
    CHECK(bass.notes.at(0) == NoteCell{.note = 24, .instrument = 0x03, .volume = 0x80});
    CHECK(bass.notes.at(2) == NoteCell{.note = 20, .instrument = 0x03, .pan = 0x00});
    CHECK(bass.effects.empty());
}

TEST_CASE("the Document builder reads every note name and each octave", "[studio][document]") {
    const Track track{.note_columns = 1, .effect_columns = 0};
    std::vector<NoteCell> notes;
    std::vector<EffectCell> effects;
    irama_studio::test::read_line("C-0 .. .. .. ..", track, notes, effects);
    CHECK(notes.at(0).note == 0);
    irama_studio::test::read_line("B-9 FE .. .. FF", track, notes, effects);
    CHECK(notes.at(0) == NoteCell{.note = 119, .instrument = 0xFE, .delay = 0xFF});
    irama_studio::test::read_line("A#4 .. .. .. ..", track, notes, effects);
    CHECK(notes.at(0).note == 58);
}
