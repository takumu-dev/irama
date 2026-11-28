// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include "app/shortcut_actions.hpp"

#include "input/chord.hpp"
#include "input/key_router.hpp"
#include "input/note_keys.hpp"
#include "input/shortcut_table.hpp"

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <utility>

using irama_studio::Key;
using irama_studio::KeyEvent;
using irama_studio::KeyRouter;
using irama_studio::NoteKeys;
using irama_studio::ShortcutId;

TEST_CASE("the octave shortcuts step the keyboard octave from 0 to 8", "[studio][shortcuts]") {
    NoteKeys keys;
    irama_studio::run_shortcut(ShortcutId::raise_octave, keys);
    CHECK(keys.octave() == 5);
    irama_studio::run_shortcut(ShortcutId::lower_octave, keys);
    irama_studio::run_shortcut(ShortcutId::lower_octave, keys);
    CHECK(keys.octave() == 3);

    keys.set_octave(0);
    irama_studio::run_shortcut(ShortcutId::lower_octave, keys);
    CHECK(keys.octave() == 0);
    keys.set_octave(8);
    irama_studio::run_shortcut(ShortcutId::raise_octave, keys);
    CHECK(keys.octave() == 8);
}

TEST_CASE("numpad / and * step the keyboard octave through the router", "[studio][shortcuts]") {
    KeyRouter router;
    const auto step = [&router](Key key) {
        const KeyEvent event{.key = std::to_underlying(key)};
        const std::optional shortcut = router.route(event, {}).shortcut;
        REQUIRE(shortcut);
        irama_studio::run_shortcut(*shortcut, router.note_keys());
        static_cast<void>(router.route(KeyEvent{.key = event.key, .down = false}, {}));
    };
    step(Key::keypad_multiply);
    CHECK(router.note_keys().octave() == 5);
    step(Key::keypad_divide);
    step(Key::keypad_divide);
    CHECK(router.note_keys().octave() == 3);
}
