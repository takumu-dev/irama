// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include "edit_helper.hpp"

#include "audio/sent_song.hpp"
#include "doc/changed_parts.hpp"
#include "doc/document.hpp"
#include "doc/edit_scope.hpp"
#include "doc/undo_history.hpp"
#include "document_builder.hpp"

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace irama_studio::test {
namespace {

[[nodiscard]] ChangedParts undone(const ChangedParts& reported, const Document& after) {
    ChangedParts parts = reported;
    if (parts.positions) {
        parts.positions =
            parts.positions->inverse(after.sequence().slots.size(), after.tracks().size());
    }
    return parts;
}

[[nodiscard]] std::string list(const std::vector<std::size_t>& numbers, std::string_view format) {
    std::string text;
    for (const std::size_t number : numbers) {
        text += std::vformat(format, std::make_format_args(number));
    }
    return text;
}

[[nodiscard]] std::string list(const std::vector<std::uint16_t>& positions) {
    std::string text;
    for (const std::uint16_t position : positions) {
        text += position == PositionMap::gone ? std::string{" gone"} : std::format(" {}", position);
    }
    return text;
}

// Undoes the edit that took `before` to the Document as it is now, then redoes it.
void check_undo_and_redo(UndoHistory& history, Document& document, SentSong& sent,
                         const Document& before, const ChangedParts& reported) {
    const Document after = document;
    REQUIRE(history.undo_count() > 0);
    const HistoryStep undo = held(history.undo(document));
    CHECK(document == before);
    CHECK(undo.parts == undone(reported, after));
    check_incremental_equals_whole(sent, document, undo.parts);

    REQUIRE(history.redo_count() > 0);
    const HistoryStep redo = held(history.redo(document));
    CHECK(document == after);
    CHECK(redo.parts == reported);
    check_incremental_equals_whole(sent, document, redo.parts);
}

} // namespace

void check_incremental_equals_whole(SentSong& sent, const Document& document,
                                    const ChangedParts& parts) {
    static_cast<void>(sent.update(document, parts));
    CHECK(sent.song() == build_engine_song(document));
}

Document check_edit(const Document& start, const Edit& edit, const ChangedParts& reported) {
    INFO("the edit " << edit.name);
    Document document = start;
    UndoHistory history{UndoHistory::studio_budget};
    SentSong sent{document};

    const std::expected<ChangedParts, Refusal> applied = history.apply(document, edit);
    INFO("refused: " << (applied ? std::string{} : applied.error()));
    REQUIRE(applied);
    CHECK(*applied == reported);
    CHECK(document != start);
    CHECK(history.undo_name() == edit.name);
    check_incremental_equals_whole(sent, document, *applied);
    check_undo_and_redo(history, document, sent, start, reported);
    return document;
}

} // namespace irama_studio::test

std::string
Catch::StringMaker<irama_studio::ChangedParts>::convert(const irama_studio::ChangedParts& parts) {
    std::string text = parts.song ? "{song" : "{";
    text += " patterns:" + irama_studio::test::list(parts.patterns, " {:03}");
    text += " instruments:" + irama_studio::test::list(parts.instruments, " {:02X}");
    if (parts.positions) {
        text += " slots:" + irama_studio::test::list(parts.positions->slots);
        text += " tracks:" + irama_studio::test::list(parts.positions->tracks);
    }
    return text + " }";
}
