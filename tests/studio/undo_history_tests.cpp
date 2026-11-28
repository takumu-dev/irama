// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include "doc/undo_history.hpp"

#include "audio/sent_song.hpp"
#include "doc/changed_parts.hpp"
#include "doc/document.hpp"
#include "doc/edit_scope.hpp"
#include "doc/edits.hpp"
#include "document_builder.hpp"
#include "edit_helper.hpp"
#include <irama/engine/voice/instrument.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <vector>

using irama::InstrumentParam;
using irama_studio::ChangedParts;
using irama_studio::Document;
using irama_studio::EditScope;
using irama_studio::GridPosition;
using irama_studio::HistoryStep;
using irama_studio::Refusal;
using irama_studio::UndoHistory;
using irama_studio::View;
using irama_studio::test::held;

namespace {

// A budget of a few steps of a few hundred bytes of text.
constexpr std::size_t small_budget = 4096;

[[nodiscard]] std::string text_of(std::size_t length, char c) {
    std::string text(length, c);
    return text;
}

void apply(UndoHistory& history, Document& document, const irama_studio::Edit& edit) {
    const std::expected<ChangedParts, Refusal> applied = history.apply(document, edit);
    REQUIRE(applied);
}

[[nodiscard]] float cutoff(const Document& document) {
    return held(document.instrument(0x01)).value(InstrumentParam::cutoff);
}

[[nodiscard]] irama_studio::Edit set_cutoff(float value) {
    return irama_studio::set_instrument_value(0x01, InstrumentParam::cutoff, value);
}

// Applies a step of a cutoff gesture, which reports instrument 01 alone, and sends it.
void apply_cutoff(UndoHistory& history, Document& document, irama_studio::SentSong& sent,
                  float value) {
    const std::expected<ChangedParts, Refusal> applied = history.apply(document, set_cutoff(value));
    REQUIRE(applied);
    CHECK(*applied == ChangedParts{.instruments = {0x01}});
    irama_studio::test::check_incremental_equals_whole(sent, document, *applied);
}

} // namespace

TEST_CASE("the history drops its oldest steps to stay within its budget", "[studio][history]") {
    Document document;
    UndoHistory history{small_budget};
    for (char c = 'a'; c <= 'j'; ++c) {
        apply(history, document, irama_studio::set_comments(text_of(300, c)));
        CHECK(history.bytes() <= small_budget);
    }
    const std::size_t kept = history.undo_count();
    CHECK(kept > 1);
    CHECK(kept < 10);

    while (history.undo(document)) {
    }
    // The steps kept are the latest, so undoing them all ends at the last step dropped.
    const auto last_dropped = static_cast<char>('j' - kept);
    CHECK(document.info().metadata.comments == text_of(300, last_dropped));
}

TEST_CASE("an edit larger than the whole budget is kept as the only step", "[studio][history]") {
    Document document;
    UndoHistory history{small_budget};
    apply(history, document, irama_studio::set_title("Small"));
    // A step keeps the part as it was before, so the edit after a long text is the large one.
    apply(history, document, irama_studio::set_comments(text_of(10'000, 'x')));
    CHECK(history.undo_count() == 2);
    apply(history, document, irama_studio::set_comments(text_of(10'000, 'y')));
    CHECK(history.undo_count() == 1);
    CHECK(history.bytes() > small_budget);

    apply(history, document, irama_studio::set_comments(text_of(10'000, 'z')));
    CHECK(history.undo_count() == 1);
    REQUIRE(history.undo(document));
    CHECK(document.info().metadata.comments == text_of(10'000, 'y'));
    CHECK(document.info().metadata.title == "Small");
    CHECK_FALSE(history.undo(document));
}

TEST_CASE("a new edit clears the steps undone", "[studio][history]") {
    Document document;
    UndoHistory history{small_budget};
    apply(history, document, irama_studio::set_title("One"));
    apply(history, document, irama_studio::set_title("Two"));
    REQUIRE(history.undo(document));
    CHECK(history.redo_count() == 1);
    CHECK(history.redo_name() == "Set Title");

    apply(history, document, irama_studio::set_author("Three"));
    CHECK(history.redo_count() == 0);
    CHECK_FALSE(history.redo_name());
    CHECK_FALSE(history.redo(document));
    CHECK(document.info().metadata.title == "One");
    CHECK(document.info().metadata.author == "Three");
}

TEST_CASE("the steps of one gesture merge into one step, and each reports what it changed",
          "[studio][history]") {
    Document document;
    const Document start = document;
    UndoHistory history{small_budget};
    irama_studio::SentSong sent{document};
    for (const float value : {1'000.0F, 2'000.0F, 3'000.0F}) {
        apply_cutoff(history, document, sent, value);
    }
    CHECK(history.undo_count() == 1);
    CHECK(history.undo_name() == "Set Cutoff");

    const HistoryStep undo = held(history.undo(document));
    CHECK(undo.parts == ChangedParts{.instruments = {0x01}});
    CHECK(document == start);
    REQUIRE(history.redo(document));
    CHECK(cutoff(document) == 3'000.0F);
}

TEST_CASE("a gesture stops merging when it ends or another value is touched", "[studio][history]") {
    Document document;
    UndoHistory history{small_budget};
    apply(history, document, set_cutoff(1'000.0F));
    history.end_gesture();
    apply(history, document, set_cutoff(2'000.0F));
    CHECK(history.undo_count() == 2);

    apply(history, document,
          irama_studio::set_instrument_value(0x01, InstrumentParam::resonance, 0.5F));
    apply(history, document, set_cutoff(3'000.0F));
    CHECK(history.undo_count() == 4);

    // An edit that isn't a gesture's ends the gesture too.
    apply(history, document, irama_studio::set_title("Between"));
    apply(history, document, set_cutoff(4'000.0F));
    CHECK(history.undo_count() == 6);
}

TEST_CASE("undo during a gesture ends it as one step, and redo brings the whole of it back",
          "[studio][history]") {
    Document document;
    const float start = cutoff(document);
    UndoHistory history{small_budget};
    apply(history, document, set_cutoff(1'000.0F));
    apply(history, document, set_cutoff(2'000.0F));
    REQUIRE(history.undo(document));
    CHECK(cutoff(document) == start);
    REQUIRE(history.redo(document));
    CHECK(cutoff(document) == 2'000.0F);

    // The gesture ended, so a later step of the same value is a step of its own.
    apply(history, document, set_cutoff(3'000.0F));
    CHECK(history.undo_count() == 2);
    REQUIRE(history.undo(document));
    CHECK(cutoff(document) == 2'000.0F);
}

TEST_CASE("a gesture brought back to where it started leaves no step", "[studio][history]") {
    Document document;
    const float start = cutoff(document);
    UndoHistory history{small_budget};
    apply(history, document, set_cutoff(1'000.0F));
    apply(history, document, set_cutoff(start));
    CHECK(history.undo_count() == 0);
    CHECK(history.bytes() == 0);
}

TEST_CASE("a refused edit records nothing and leaves the Document as it was", "[studio][history]") {
    const Document start = irama_studio::test::test_song();
    Document document = start;
    UndoHistory history{small_budget};
    const auto refused = history.apply(document, irama_studio::delete_instrument(0x04));
    REQUIRE_FALSE(refused);
    CHECK(refused.error() == "Instrument 04 is empty");
    CHECK(document == start);
    CHECK(history.undo_count() == 0);
}

TEST_CASE("an edit that breaks a limit after it has written is put back whole",
          "[studio][history]") {
    const Document start = irama_studio::test::test_song();
    Document document = start;
    UndoHistory history{small_budget};
    const irama_studio::Edit partway{
        .name = "Partway",
        .function = [](EditScope& scope) -> std::expected<void, Refusal> {
            scope.info().metadata.title = "Changed";
            scope.instrument(0x02)->name = "Changed";
            scope.parameters().push_back({.name = "Intensity"});
            return {};
        },
    };
    const auto broken = history.apply(document, partway);
    REQUIRE_FALSE(broken);
    CHECK(broken.error() == "There is already a song parameter named Intensity");
    CHECK(document == start);
    CHECK(history.undo_count() == 0);
}

TEST_CASE("an edit that refuses after it has written is put back whole", "[studio][history]") {
    const Document start = irama_studio::test::test_song();
    Document document = start;
    UndoHistory history{small_budget};
    const irama_studio::Edit refusing{
        .name = "Refusing",
        .function = [](EditScope& scope) -> std::expected<void, Refusal> {
            scope.info().seed = 7;
            return std::unexpected{"No"};
        },
    };
    CHECK_FALSE(history.apply(document, refusing));
    CHECK(document == start);
    CHECK(history.undo_count() == 0);
}

TEST_CASE("an edit that changes nothing records nothing and reports nothing", "[studio][history]") {
    Document document;
    UndoHistory history{small_budget};
    const auto applied = history.apply(document, irama_studio::set_title(""));
    REQUIRE(applied);
    CHECK(applied->empty());
    CHECK(history.undo_count() == 0);
}

TEST_CASE("undo restores the view before the edit and redo the view after it",
          "[studio][history]") {
    Document document;
    UndoHistory history{small_budget};
    const View before{.slot = 0, .cursor = {.line = 3, .track = 1}};
    const View after{
        .slot = 0,
        .cursor = {.line = 4, .track = 1},
        .selection_anchor = GridPosition{.line = 0, .track = 2, .field = 1},
    };
    REQUIRE(history.apply(document, irama_studio::set_title("Viewed"),
                          {.before = before, .after = after}));
    apply(history, document, irama_studio::set_author("Unviewed"));

    CHECK_FALSE(held(history.undo(document)).view);
    CHECK(held(history.undo(document)).view == before);
    CHECK(held(history.redo(document)).view == after);
}

TEST_CASE("undo reports the position map turned around, and redo the map forward",
          "[studio][history]") {
    Document document;
    UndoHistory history{small_budget};
    // Inserts a slot before the first, and moves the first track to the end. Every track's cells
    // are empty, so the pattern itself is left as it was.
    const irama_studio::Edit moving{
        .name = "Moving",
        .function = [](EditScope& scope) -> std::expected<void, Refusal> {
            std::vector<std::uint16_t>& slots = scope.sequence().slots;
            slots.insert(slots.begin(), 0);
            std::vector<irama_studio::Track>& tracks = scope.tracks();
            std::ranges::rotate(tracks, tracks.begin() + 1);
            std::vector<irama_studio::TrackBlock>& blocks = scope.pattern(0)->blocks;
            std::ranges::rotate(blocks, blocks.begin() + 1);
            scope.set_positions({.slots = {1}, .tracks = {3, 0, 1, 2}});
            return {};
        },
    };
    const irama_studio::PositionMap forward{.slots = {1}, .tracks = {3, 0, 1, 2}};
    const Document after =
        irama_studio::test::check_edit(document, moving, {.song = true, .positions = forward});
    CHECK(after.sequence().slots.size() == 2);
    CHECK(forward.inverse(2, 4) ==
          irama_studio::PositionMap{.slots = {irama_studio::PositionMap::gone, 0},
                                    .tracks = {1, 2, 3, 0}});
}
