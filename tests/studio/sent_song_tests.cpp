// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include "audio/sent_song.hpp"

#include "doc/changed_parts.hpp"
#include "doc/document.hpp"
#include "doc/edit_scope.hpp"
#include "doc/edits.hpp"
#include "document_builder.hpp"
#include <irama/engine/model/change_set.hpp>
#include <irama/engine/model/engine_song.hpp>
#include <irama/engine/model/pattern_part.hpp>
#include <irama/engine/voice/instrument.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <expected>
#include <vector>

using irama::InstrumentParam;
using irama_studio::ChangedParts;
using irama_studio::Document;
using irama_studio::EditRun;
using irama_studio::SentSong;

namespace {

// Applies an edit outside any history, and returns the parts it changed.
[[nodiscard]] ChangedParts edit(Document& document, const irama_studio::Edit& edit) {
    const auto run = EditRun::run(document, edit.function);
    REQUIRE(run);
    return run->parts();
}

} // namespace

TEST_CASE("the song built from a Document holds what the player plays of it", "[studio][sent]") {
    const Document document = irama_studio::test::test_song();
    const irama::EngineSong song = irama_studio::build_engine_song(document);
    REQUIRE(song.song() != nullptr);
    CHECK(song.song()->bpm100() == 12'000);
    CHECK(song.song()->seed() == document.info().seed);
    CHECK(std::ranges::equal(song.song()->note_columns(), std::array<std::uint8_t, 2>{2, 1}));
    CHECK(std::vector<std::uint16_t>(song.song()->sequence().begin(),
                                     song.song()->sequence().end()) ==
          std::vector<std::uint16_t>{0, 1, 0, 7});

    const irama::PatternPart* pattern = song.pattern(0);
    REQUIRE(pattern != nullptr);
    CHECK(pattern->lines() == 8);
    CHECK(pattern->columns() == 3);
    // Each line holds the lead's two note columns, then the bass's.
    CHECK(pattern->line(0).front() ==
          irama::NoteCell{.note = 48, .instrument = 0x01, .volume = 0x40});
    CHECK(pattern->line(0).back() ==
          irama::NoteCell{.note = 24, .instrument = 0x03, .volume = 0x80});
    CHECK(pattern->line(1).subspan(1).front().note == 52);
    CHECK(song.pattern(1) != nullptr);
    CHECK(song.pattern(2) == nullptr);
    CHECK(song.pattern(7) != nullptr);

    CHECK(song.instrument(0x01) != nullptr);
    CHECK(song.instrument(0x03) != nullptr);
    CHECK(song.instrument(0x04) == nullptr);
}

TEST_CASE("an edit of what the player doesn't hold sends nothing", "[studio][sent]") {
    Document document = irama_studio::test::test_song();
    SentSong sent{document};
    CHECK(sent.update(document, edit(document, irama_studio::set_title("Unheard"))) == nullptr);
    CHECK(sent.update(document, edit(document, irama_studio::add_marker(1, {.name = "M"}))) ==
          nullptr);
    CHECK(sent.update(document, edit(document, irama_studio::rename_instrument(2, "Renamed"))) ==
          nullptr);
    CHECK(sent.song() == irama_studio::build_engine_song(document));
}

TEST_CASE("an edit sends only the parts that differ from those last sent", "[studio][sent]") {
    Document document = irama_studio::test::test_song();
    SentSong sent{document};
    const ChangedParts parts =
        edit(document, irama_studio::set_instrument_value(2, InstrumentParam::gain, -30.0F));
    ChangedParts reported = parts;
    reported.song = true;
    reported.patterns = {0, 7};
    reported.instruments = {1, 2, 3};
    const irama::ChangeSetPtr changes = sent.update(document, reported);
    REQUIRE(changes != nullptr);
    CHECK_FALSE(changes->replaces_song());
    CHECK(changes->patterns().empty());
    REQUIRE(changes->instruments().size() == 1);
    CHECK(changes->instruments().front().number == 2);
    REQUIRE(changes->instruments().front().part != nullptr);
    CHECK(*changes->instruments().front().part == *sent.song().instrument(2));
    CHECK(changes->slot_map().empty());
    CHECK(sent.song() == irama_studio::build_engine_song(document));

    // Sent once, the same part isn't sent again.
    CHECK(sent.update(document, parts) == nullptr);
}

TEST_CASE("an emptied number is sent as an empty entry", "[studio][sent]") {
    Document document = irama_studio::test::test_song();
    SentSong sent{document};
    const irama::ChangeSetPtr changes =
        sent.update(document, edit(document, irama_studio::delete_instrument(3)));
    REQUIRE(changes != nullptr);
    REQUIRE(changes->instruments().size() == 1);
    CHECK(changes->instruments().front().number == 3);
    CHECK(changes->instruments().front().part == nullptr);
    CHECK(sent.song().instrument(3) == nullptr);
}

TEST_CASE("a change set carries the position map of an edit that moves slots or tracks",
          "[studio][sent]") {
    Document document;
    SentSong sent{document};
    const ChangedParts parts{
        .song = true,
        .positions = irama_studio::PositionMap{.slots = {0}, .tracks = {1, 0, 2, 3}},
    };
    const irama::ChangeSetPtr changes = sent.update(document, parts);
    REQUIRE(changes != nullptr);
    CHECK_FALSE(changes->replaces_song());
    CHECK(std::vector<std::uint16_t>(changes->slot_map().begin(), changes->slot_map().end()) ==
          std::vector<std::uint16_t>{0});
    CHECK(std::vector<std::uint16_t>(changes->track_map().begin(), changes->track_map().end()) ==
          std::vector<std::uint16_t>{1, 0, 2, 3});
}

TEST_CASE("the whole song as sent loads a player with every part", "[studio][sent]") {
    Document document = irama_studio::test::test_song();
    SentSong sent{document};
    static_cast<void>(sent.update(
        document,
        edit(document, irama_studio::set_instrument_value(1, InstrumentParam::cutoff, 500.0F))));
    irama::ChangeSetPtr whole = sent.whole_song();
    REQUIRE(whole != nullptr);
    CHECK(whole->replaces_song());
    CHECK(whole->patterns().size() == 3);
    CHECK(whole->instruments().size() == 3);
    irama::EngineSong loaded;
    loaded.apply(*whole);
    CHECK(loaded == irama_studio::build_engine_song(document));
}
