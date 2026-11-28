// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include "app/song_editor.hpp"

#include "app/shortcut_actions.hpp"
#include "audio/audio_device.hpp"
#include "audio/audio_engine.hpp"
#include "audio/sent_song.hpp"
#include "doc/document.hpp"
#include "doc/edits.hpp"
#include "document_builder.hpp"
#include "input/key_router.hpp"
#include "input/note_keys.hpp"
#include "input/shortcut_table.hpp"
#include <irama/engine/voice/instrument.hpp>

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <span>

using irama::InstrumentParam;
using irama_studio::AudioBackend;
using irama_studio::AudioEngine;
using irama_studio::Document;
using irama_studio::KeyEvent;
using irama_studio::KeyFocus;
using irama_studio::KeyRouter;
using irama_studio::ShortcutId;
using irama_studio::SongEditor;

namespace {

// USB HID usage ID of the key that types Z on a US layout.
constexpr int z_key = 29;

[[nodiscard]] float cutoff(const SongEditor& editor) {
    return irama_studio::test::held(editor.document().instrument(0x01))
        .value(InstrumentParam::cutoff);
}

} // namespace

TEST_CASE("the song editor keeps each change's parts sent, edit, undo and redo alike",
          "[studio][editor]") {
    SongEditor editor;
    AudioEngine engine{std::span<const AudioBackend>{}, editor.document()};
    REQUIRE(editor.apply(irama_studio::set_instrument_value(0x01, InstrumentParam::cutoff, 700.0F),
                         engine));
    CHECK(engine.sent_song().song() == irama_studio::build_engine_song(editor.document()));
    static_cast<void>(editor.undo(engine));
    CHECK(editor.document() == Document{});
    CHECK(engine.sent_song().song() == irama_studio::build_engine_song(Document{}));
    static_cast<void>(editor.redo(engine));
    CHECK(engine.sent_song().song() == irama_studio::build_engine_song(editor.document()));
    CHECK(cutoff(editor) == 700.0F);
}

TEST_CASE("a refused edit leaves its reason until the next change", "[studio][editor]") {
    SongEditor editor;
    AudioEngine engine{std::span<const AudioBackend>{}, editor.document()};
    CHECK(editor.refusal().empty());
    CHECK_FALSE(editor.apply(irama_studio::rename_instrument(0x09, "Nine"), engine));
    CHECK(editor.refusal() == "Instrument 09 is empty");
    CHECK(editor.history().undo_count() == 0);
    REQUIRE(editor.apply(irama_studio::set_title("Fine"), engine));
    CHECK(editor.refusal().empty());
}

TEST_CASE("the undo and redo shortcuts step through the history", "[studio][editor]") {
    SongEditor editor;
    AudioEngine engine{std::span<const AudioBackend>{}, editor.document()};
    irama_studio::NoteKeys keys;
    REQUIRE(editor.apply(irama_studio::set_title("Titled"), engine));
    irama_studio::run_shortcut(ShortcutId::undo, keys, editor, engine);
    CHECK(editor.document().info().metadata.title.empty());
    irama_studio::run_shortcut(ShortcutId::redo, keys, editor, engine);
    CHECK(editor.document().info().metadata.title == "Titled");
}

TEST_CASE("Ctrl+Z undoes and Ctrl+Y and Ctrl+Shift+Z redo, unless a text field has focus",
          "[studio][editor]") {
    const auto route = [](KeyRouter& router, char32_t character, bool shift,
                          const KeyFocus& focus) {
        const KeyEvent down{
            .key = z_key,
            .character = character,
            .modifiers = {.ctrl = true, .shift = shift},
        };
        const std::optional<ShortcutId> shortcut = router.route(down, focus).shortcut;
        static_cast<void>(router.route(KeyEvent{.key = z_key, .down = false}, focus));
        return shortcut;
    };
    KeyRouter router;
    CHECK(route(router, U'z', false, {}) == ShortcutId::undo);
    CHECK(route(router, U'y', false, {}) == ShortcutId::redo);
    CHECK(route(router, U'z', true, {}) == ShortcutId::redo);
    CHECK_FALSE(route(router, U'z', false, {.text_field = true}));
    CHECK_FALSE(route(router, U'y', false, {.text_field = true}));
}

TEST_CASE("the Edit menu's entries show the undo and redo chords from the table",
          "[studio][editor]") {
    CHECK(irama_studio::chords_text(irama_studio::find_shortcut(ShortcutId::undo)) == "Ctrl+Z");
    CHECK(irama_studio::chords_text(irama_studio::find_shortcut(ShortcutId::redo)) ==
          "Ctrl+Y, Ctrl+Shift+Z");
}
