// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

// Every kind of edit through the edit helper, and the edits each kind refuses.

#include "doc/edits.hpp"

#include "doc/changed_parts.hpp"
#include "doc/document.hpp"
#include "doc/edit_scope.hpp"
#include "document_builder.hpp"
#include "edit_helper.hpp"
#include <irama/engine/voice/instrument.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <expected>
#include <limits>
#include <string>
#include <vector>

using irama::InstrumentParam;
using irama_studio::ChangedParts;
using irama_studio::Document;
using irama_studio::EditRun;
using irama_studio::Marker;
using irama_studio::SongParameter;
using irama_studio::test::check_edit;
using irama_studio::test::held;
using irama_studio::test::test_song;

namespace {

const ChangedParts song_part{.song = true};

[[nodiscard]] ChangedParts pattern_part(std::size_t number) { return {.patterns = {number}}; }

[[nodiscard]] ChangedParts instrument_part(std::size_t number) { return {.instruments = {number}}; }

// The refusal of an edit applied to `document`, which must be left as it was.
[[nodiscard]] std::string refusal(const Document& document, const irama_studio::Edit& edit) {
    Document edited = document;
    const auto run = EditRun::run(edited, edit.function);
    CHECK(edited == document);
    REQUIRE_FALSE(run);
    return run.error();
}

} // namespace

TEST_CASE("the metadata edits set the title, author and comments", "[studio][edits]") {
    const Document song = test_song();
    CHECK(check_edit(song, irama_studio::set_title("Night"), song_part).info().metadata.title ==
          "Night");
    CHECK(check_edit(song, irama_studio::set_author("Someone"), song_part).info().metadata.author ==
          "Someone");
    CHECK(check_edit(song, irama_studio::set_comments("Line one\nLine two"), song_part)
              .info()
              .metadata.comments == "Line one\nLine two");
}

TEST_CASE("the seed edit sets the seed", "[studio][edits]") {
    CHECK(check_edit(test_song(), irama_studio::set_seed(42), song_part).info().seed == 42);
}

TEST_CASE("the song parameter edits add, change and delete song parameters", "[studio][edits]") {
    const Document song = test_song();
    const SongParameter danger{.name = "Danger", .min = -1.0F, .max = 1.0F, .default_value = 0.0F};
    const Document added = check_edit(song, irama_studio::add_song_parameter(danger), song_part);
    CHECK(added.parameters() == std::vector<SongParameter>{song.parameters().front(), danger});

    const SongParameter calm{.name = "Calm", .min = 0.0F, .max = 10.0F, .default_value = 10.0F};
    const Document changed =
        check_edit(added, irama_studio::change_song_parameter(0, calm), song_part);
    CHECK(changed.parameters() == std::vector<SongParameter>{calm, danger});

    const Document deleted = check_edit(added, irama_studio::delete_song_parameter(0), song_part);
    CHECK(deleted.parameters() == std::vector<SongParameter>{danger});
}

TEST_CASE("the song parameter edits refuse names and ranges game code couldn't use",
          "[studio][edits]") {
    const Document song = test_song();
    const SongParameter good{.name = "Danger", .min = 0.0F, .max = 2.0F, .default_value = 1.0F};
    SongParameter same_name = good;
    same_name.name = "Intensity";
    CHECK(refusal(song, irama_studio::add_song_parameter(same_name)) ==
          "There is already a song parameter named Intensity");
    SongParameter spaced = good;
    spaced.name = " Danger";
    CHECK(refusal(song, irama_studio::add_song_parameter(spaced)) ==
          "A song parameter's name isn't empty and has no space at either end");
    SongParameter empty = good;
    empty.name.clear();
    CHECK_FALSE(refusal(song, irama_studio::add_song_parameter(empty)).empty());
    SongParameter backwards = good;
    backwards.min = 3.0F;
    CHECK(refusal(song, irama_studio::add_song_parameter(backwards)) ==
          "A song parameter's minimum is below its maximum");
    SongParameter outside = good;
    outside.default_value = 5.0F;
    CHECK(refusal(song, irama_studio::add_song_parameter(outside)) ==
          "A song parameter's default is in its range");
    CHECK(refusal(song, irama_studio::change_song_parameter(1, good)) ==
          "There is no song parameter 2");
    CHECK(refusal(song, irama_studio::delete_song_parameter(1)) == "There is no song parameter 2");

    irama_studio::test::DocumentBuilder full;
    for (std::size_t index = 0; index < Document::max_parameters; ++index) {
        full.parameter({.name = "P" + std::to_string(index)});
    }
    CHECK(refusal(full.build(), irama_studio::add_song_parameter(good)) ==
          "A song has at most 64 song parameters");
}

TEST_CASE("the marker edits add, rename and delete a pattern's markers", "[studio][edits]") {
    const Document song = test_song();
    const Document added = check_edit(
        song, irama_studio::add_marker(0, {.line = 5, .name = "Build"}), pattern_part(0));
    CHECK(held(added.pattern(0)).head.markers ==
          std::vector<Marker>{{.line = 0, .name = "Drop"}, {.line = 5, .name = "Build"}});

    const Document before =
        check_edit(song, irama_studio::add_marker(7, {.line = 3, .name = "Fill"}), pattern_part(7));
    CHECK(held(before.pattern(7)).head.markers ==
          std::vector<Marker>{{.line = 3, .name = "Fill"}, {.line = 12, .name = "Fill"}});

    const Document renamed =
        check_edit(song, irama_studio::rename_marker(0, 0, "Break"), pattern_part(0));
    CHECK(held(renamed.pattern(0)).head.markers ==
          std::vector<Marker>{{.line = 0, .name = "Break"}});

    const Document deleted = check_edit(song, irama_studio::delete_marker(7, 12), pattern_part(7));
    CHECK(held(deleted.pattern(7)).head.markers.empty());
}

TEST_CASE("the marker edits refuse lines that are taken, missing or past the end",
          "[studio][edits]") {
    const Document song = test_song();
    CHECK(refusal(song, irama_studio::add_marker(0, {.line = 0, .name = "Again"})) ==
          "There is already a marker on line 0");
    CHECK(refusal(song, irama_studio::add_marker(0, {.line = 8, .name = "Late"})) ==
          "Pattern 000 has 8 lines");
    CHECK(refusal(song, irama_studio::add_marker(0, {.line = 1, .name = ""})) ==
          "A marker has a name");
    CHECK(refusal(song, irama_studio::add_marker(2, {.line = 0, .name = "Lost"})) ==
          "There is no pattern 002");
    CHECK(refusal(song, irama_studio::rename_marker(0, 1, "Nothing")) ==
          "Pattern 000 has no marker on line 1");
    CHECK(refusal(song, irama_studio::rename_marker(0, 0, "")) == "A marker has a name");
    CHECK(refusal(song, irama_studio::delete_marker(1, 0)) ==
          "Pattern 001 has no marker on line 0");
}

TEST_CASE("the instrument edits make, clone, delete and rename instruments", "[studio][edits]") {
    const Document song = test_song();
    const Document made =
        check_edit(song, irama_studio::new_instrument(0x10), instrument_part(0x10));
    CHECK(made.instrument(0x10) == irama_studio::default_instrument());

    const Document renamed =
        check_edit(song, irama_studio::rename_instrument(0x02, "Strings"), instrument_part(0x02));
    CHECK(held(renamed.instrument(0x02)).name == "Strings");

    const Document cloned =
        check_edit(song, irama_studio::clone_instrument(0x02, 0xFE), instrument_part(0xFE));
    CHECK(cloned.instrument(0xFE) == song.instrument(0x02));

    // Deleting leaves the notes that name the instrument where they are.
    const Document deleted =
        check_edit(song, irama_studio::delete_instrument(0x03), instrument_part(0x03));
    CHECK_FALSE(deleted.instrument(0x03));
    CHECK(deleted.pattern(0) == song.pattern(0));
}

TEST_CASE("the instrument edits refuse numbers that are empty, filled or out of range",
          "[studio][edits]") {
    const Document song = test_song();
    CHECK(refusal(song, irama_studio::new_instrument(0x02)) == "Instrument 02 is already filled");
    CHECK(refusal(song, irama_studio::new_instrument(0x00)) ==
          "Instrument numbers are from 01 to FE");
    CHECK(refusal(song, irama_studio::new_instrument(0xFF)) ==
          "Instrument numbers are from 01 to FE");
    CHECK(refusal(song, irama_studio::clone_instrument(0x04, 0x05)) == "Instrument 04 is empty");
    CHECK(refusal(song, irama_studio::clone_instrument(0x01, 0x03)) ==
          "Instrument 03 is already filled");
    CHECK(refusal(song, irama_studio::delete_instrument(0x04)) == "Instrument 04 is empty");
    CHECK(refusal(song, irama_studio::rename_instrument(0x04, "None")) == "Instrument 04 is empty");
    CHECK(refusal(song, irama_studio::set_instrument_value(0x04, InstrumentParam::cutoff, 1.0F)) ==
          "Instrument 04 is empty");
}

TEST_CASE("an instrument value edit clamps and rounds the value as its parameter says",
          "[studio][edits]") {
    const Document song = test_song();
    const Document set = check_edit(
        song, irama_studio::set_instrument_value(0x02, InstrumentParam::cutoff, 1'000.0F),
        instrument_part(0x02));
    CHECK(held(set.instrument(0x02)).value(InstrumentParam::cutoff) == 1'000.0F);
    CHECK(set.instrument(0x01) == song.instrument(0x01));

    const Document clamped =
        check_edit(song, irama_studio::set_instrument_value(0x01, InstrumentParam::cutoff, -5.0F),
                   instrument_part(0x01));
    CHECK(held(clamped.instrument(0x01)).value(InstrumentParam::cutoff) == 20.0F);

    const Document rounded = check_edit(
        song, irama_studio::set_instrument_value(0x01, InstrumentParam::osc1_coarse, 2.6F),
        instrument_part(0x01));
    CHECK(held(rounded.instrument(0x01)).value(InstrumentParam::osc1_coarse) == 3.0F);

    CHECK(refusal(song, irama_studio::set_instrument_value(
                            0x01, InstrumentParam::cutoff,
                            std::numeric_limits<float>::quiet_NaN())) == "Cutoff takes a number");
}

TEST_CASE("each edit is named for the Edit menu", "[studio][edits]") {
    CHECK(irama_studio::set_title("").name == "Set Title");
    CHECK(irama_studio::set_author("").name == "Set Author");
    CHECK(irama_studio::set_comments("").name == "Set Comments");
    CHECK(irama_studio::set_seed(0).name == "Set Seed");
    CHECK(irama_studio::add_song_parameter({}).name == "Add Song Parameter");
    CHECK(irama_studio::change_song_parameter(0, {}).name == "Change Song Parameter");
    CHECK(irama_studio::delete_song_parameter(0).name == "Delete Song Parameter");
    CHECK(irama_studio::add_marker(0, {}).name == "Add Marker");
    CHECK(irama_studio::rename_marker(0, 0, "").name == "Rename Marker");
    CHECK(irama_studio::delete_marker(0, 0).name == "Delete Marker");
    CHECK(irama_studio::new_instrument(1).name == "New Instrument");
    CHECK(irama_studio::clone_instrument(1, 2).name == "Clone Instrument");
    CHECK(irama_studio::delete_instrument(1).name == "Delete Instrument");
    CHECK(irama_studio::rename_instrument(1, "").name == "Rename Instrument");
    CHECK(irama_studio::set_instrument_value(1, InstrumentParam::cutoff, 0.0F).name ==
          "Set Cutoff");
}

TEST_CASE("only an instrument value edit is a step of a gesture, one for each value",
          "[studio][edits]") {
    CHECK_FALSE(irama_studio::set_title("").gesture);
    CHECK_FALSE(irama_studio::rename_instrument(1, "").gesture);
    const auto cutoff = irama_studio::set_instrument_value(1, InstrumentParam::cutoff, 0.0F);
    REQUIRE(cutoff.gesture);
    CHECK(cutoff.gesture ==
          irama_studio::set_instrument_value(1, InstrumentParam::cutoff, 9.0F).gesture);
    CHECK(cutoff.gesture !=
          irama_studio::set_instrument_value(1, InstrumentParam::resonance, 0.0F).gesture);
    CHECK(cutoff.gesture !=
          irama_studio::set_instrument_value(2, InstrumentParam::cutoff, 0.0F).gesture);
}
