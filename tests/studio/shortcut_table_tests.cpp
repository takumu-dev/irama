// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include "input/shortcut_table.hpp"

#include "input/chord.hpp"
#include "input/key_map.hpp"

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <format>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <utility>

using irama_studio::Chord;
using irama_studio::Key;
using irama_studio::Shortcut;
using irama_studio::ShortcutContext;
using irama_studio::ShortcutId;

namespace {

constexpr std::array all_contexts{
    ShortcutContext::global,
    ShortcutContext::pattern,
    ShortcutContext::sequence,
};

// Every named key that a chord with no modifier may use.
constexpr std::array unmodified_keys{
    Key::space,
    Key::enter,
    Key::escape,
    Key::tab,
    Key::up,
    Key::down,
    Key::left,
    Key::right,
    Key::page_up,
    Key::page_down,
    Key::home,
    Key::end,
    Key::insert,
    Key::delete_key,
    Key::backspace,
    Key::f1,
    Key::f2,
    Key::f3,
    Key::f4,
    Key::f5,
    Key::f6,
    Key::f7,
    Key::f8,
    Key::f9,
    Key::f10,
    Key::f11,
    Key::f12,
    Key::keypad_divide,
    Key::keypad_multiply,
    Key::keypad_minus,
    Key::keypad_plus,
    Key::keypad_enter,
    Key::keypad_period,
    Key::keypad_0,
    Key::keypad_1,
    Key::keypad_2,
    Key::keypad_3,
    Key::keypad_4,
    Key::keypad_5,
    Key::keypad_6,
    Key::keypad_7,
    Key::keypad_8,
    Key::keypad_9,
    Key::scroll_lock,
    Key::right_ctrl,
    Key::right_alt,
};

// The first chord that two shortcuts whose contexts overlap share, or that one shortcut holds
// twice, written as "<first> and <second>: <chord>".
[[nodiscard]] std::optional<std::string> find_clash(std::span<const Shortcut> table) {
    for (const auto [i, first] : std::views::enumerate(table)) {
        for (const Shortcut& second : table | std::views::drop(i)) {
            if (!irama_studio::contexts_overlap(first.context, second.context)) {
                continue;
            }
            // A shortcut's own chords are compared each with the ones after it.
            const bool same = &first == &second;
            for (const auto [a, chord] : std::views::enumerate(first.chords)) {
                if (std::ranges::contains(second.chords | std::views::drop(same ? a + 1 : 0),
                                          chord)) {
                    return std::format("{} and {}: {}", first.name, second.name,
                                       irama_studio::chord_text(chord));
                }
            }
        }
    }
    return std::nullopt;
}

// Whether an unmodified chord uses one of the named keys set aside for it, on no note key's
// position and with no character.
[[nodiscard]] bool clear_of_note_keys(const Chord& chord) {
    return std::ranges::contains(unmodified_keys, chord.key) &&
           !irama_studio::note_for_key(std::to_underlying(chord.key), irama_studio::min_octave) &&
           chord.character == '\0';
}

} // namespace

TEST_CASE("every shortcut has a name, at least one chord, and an entry of its own",
          "[studio][shortcuts]") {
    const std::span<const Shortcut> table = irama_studio::shortcut_table();
    REQUIRE_FALSE(table.empty());
    for (const Shortcut& shortcut : table) {
        INFO(shortcut.name);
        CHECK_FALSE(shortcut.name.empty());
        CHECK_FALSE(shortcut.chords.empty());
        CHECK(std::ranges::count(table, shortcut.id, &Shortcut::id) == 1);
        CHECK(&irama_studio::find_shortcut(shortcut.id) == &shortcut);
    }
}

TEST_CASE("every chord in the table is one Studio allows", "[studio][shortcuts]") {
    for (const Shortcut& shortcut : irama_studio::shortcut_table()) {
        for (const Chord& chord : shortcut.chords) {
            INFO(shortcut.name << ": " << irama_studio::chord_text(chord));
            CHECK(irama_studio::is_allowed(chord));
        }
    }
}

TEST_CASE("no unmodified chord in the table sits on a note key or a character the note keys type",
          "[studio][shortcuts]") {
    for (const Shortcut& shortcut : irama_studio::shortcut_table()) {
        for (const Chord& chord : shortcut.chords) {
            if (!chord.modifiers.none()) {
                continue;
            }
            INFO(shortcut.name << ": " << irama_studio::chord_text(chord));
            CHECK(clear_of_note_keys(chord));
        }
    }
}

TEST_CASE("no two shortcuts whose contexts overlap share a chord", "[studio][shortcuts]") {
    CHECK(find_clash(irama_studio::shortcut_table()) == std::nullopt);
}

TEST_CASE("a chord shared where contexts overlap is a clash", "[studio][shortcuts]") {
    constexpr std::array f5{Chord{.key = Key::f5}};
    constexpr std::array f6_and_f5{Chord{.key = Key::f6}, Chord{.key = Key::f5}};
    constexpr std::array f5_twice{Chord{.key = Key::f5}, Chord{.key = Key::f5}};
    const auto entry = [](std::string_view name, ShortcutContext context,
                          std::span<const Chord> chords) {
        return Shortcut{.name = name, .context = context, .chords = chords};
    };
    const std::array global_and_pattern{
        entry("Global", ShortcutContext::global, f5),
        entry("Pattern", ShortcutContext::pattern, f6_and_f5),
    };
    CHECK(find_clash(global_and_pattern) == "Global and Pattern: F5");
    const std::array two_patterns{
        entry("First", ShortcutContext::pattern, f6_and_f5),
        entry("Second", ShortcutContext::pattern, f5),
    };
    CHECK(find_clash(two_patterns) == "First and Second: F5");
    const std::array pattern_and_sequence{
        entry("Pattern", ShortcutContext::pattern, f5),
        entry("Sequence", ShortcutContext::sequence, f6_and_f5),
    };
    CHECK(find_clash(pattern_and_sequence) == std::nullopt);
    const std::array twice{entry("Twice", ShortcutContext::sequence, f5_twice)};
    CHECK(find_clash(twice) == "Twice and Twice: F5");
}

TEST_CASE("a global shortcut overlaps every context, and the window contexts only themselves",
          "[studio][shortcuts]") {
    using irama_studio::contexts_overlap;
    for (const ShortcutContext context : all_contexts) {
        CHECK(contexts_overlap(ShortcutContext::global, context));
        CHECK(contexts_overlap(context, ShortcutContext::global));
        CHECK(contexts_overlap(context, context));
    }
    CHECK_FALSE(contexts_overlap(ShortcutContext::pattern, ShortcutContext::sequence));
    CHECK_FALSE(contexts_overlap(ShortcutContext::sequence, ShortcutContext::pattern));
}

TEST_CASE("an unmodified chord may use only the named keys set aside for it",
          "[studio][shortcuts]") {
    using irama_studio::is_allowed;
    for (const Key key : unmodified_keys) {
        INFO("usage ID " << static_cast<int>(std::to_underlying(key)));
        CHECK(is_allowed(Chord{.key = key}));
    }
    // Plain letters, digits and punctuation belong to the note keys and to entry, whether or not a
    // note key types them.
    for (const char character : {'z', 'q', '2', ',', '/', 'a', 'f', '1', '-', '['}) {
        INFO("character " << character);
        CHECK_FALSE(is_allowed(Chord{.character = character}));
    }
}

TEST_CASE("a modified chord may use a character or any named key but a lone modifier",
          "[studio][shortcuts]") {
    using irama_studio::is_allowed;
    CHECK(is_allowed(Chord{.character = 'z', .modifiers = {.ctrl = true}}));
    CHECK(is_allowed(Chord{.character = 'z', .modifiers = {.ctrl = true, .shift = true}}));
    CHECK(is_allowed(Chord{.character = 'a', .modifiers = {.shift = true}}));
    CHECK(is_allowed(Chord{.key = Key::space, .modifiers = {.shift = true}}));
    CHECK(is_allowed(Chord{.key = Key::page_up, .modifiers = {.ctrl = true}}));
    CHECK(is_allowed(Chord{.key = Key::f1, .modifiers = {.alt = true}}));
    CHECK_FALSE(is_allowed(Chord{.key = Key::right_alt, .modifiers = {.ctrl = true}}));
    CHECK_FALSE(is_allowed(Chord{.key = Key::right_ctrl, .modifiers = {.shift = true}}));
}

TEST_CASE("a chord names a key or a character, never both or neither", "[studio][shortcuts]") {
    using irama_studio::is_allowed;
    CHECK_FALSE(is_allowed(Chord{}));
    CHECK_FALSE(is_allowed(Chord{.modifiers = {.ctrl = true}}));
    CHECK_FALSE(is_allowed(Chord{.key = Key::f5, .character = 'z', .modifiers = {.ctrl = true}}));
}

TEST_CASE("a chord is written with its modifiers first and its key's name", "[studio][shortcuts]") {
    using irama_studio::chord_text;
    CHECK(chord_text(Chord{.character = 'z', .modifiers = {.ctrl = true}}) == "Ctrl+Z");
    CHECK(chord_text(Chord{.character = 'z', .modifiers = {.ctrl = true, .shift = true}}) ==
          "Ctrl+Shift+Z");
    CHECK(chord_text(
              Chord{.character = 'v', .modifiers = {.ctrl = true, .alt = true, .shift = true}}) ==
          "Ctrl+Alt+Shift+V");
    CHECK(chord_text(Chord{.character = ',', .modifiers = {.alt = true}}) == "Alt+,");
    CHECK(chord_text(Chord{.key = Key::space, .modifiers = {.shift = true}}) == "Shift+Space");
    CHECK(chord_text(Chord{.key = Key::page_down, .modifiers = {.ctrl = true}}) ==
          "Ctrl+Page Down");
    CHECK(chord_text(Chord{.key = Key::keypad_divide}) == "Numpad /");
    CHECK(chord_text(Chord{.key = Key::keypad_multiply}) == "Numpad *");
    CHECK(chord_text(Chord{.key = Key::keypad_minus}) == "Numpad -");
    CHECK(chord_text(Chord{.key = Key::keypad_7}) == "Numpad 7");
    CHECK(chord_text(Chord{.key = Key::right_ctrl}) == "Right Ctrl");
    CHECK(chord_text(Chord{.key = Key::right_alt}) == "Right Alt");
    CHECK(chord_text(Chord{.key = Key::scroll_lock}) == "Scroll Lock");
    CHECK(chord_text(Chord{.key = Key::f5}) == "F5");
    CHECK(chord_text(Chord{.key = Key::f12}) == "F12");
    CHECK(chord_text(Chord{.key = Key::escape}) == "Esc");
    CHECK(chord_text(Chord{.key = Key::delete_key}) == "Delete");
}

TEST_CASE("a shortcut's chords are written in order, separated by commas", "[studio][shortcuts]") {
    constexpr std::array chords{Chord{.key = Key::right_ctrl}, Chord{.key = Key::f5}};
    const Shortcut shortcut{.name = "Play song", .chords = chords};
    CHECK(irama_studio::chords_text(shortcut) == "Right Ctrl, F5");
}

TEST_CASE("numpad / lowers the keyboard octave and numpad * raises it, as global shortcuts",
          "[studio][shortcuts]") {
    const Shortcut& lower = irama_studio::find_shortcut(ShortcutId::lower_octave);
    CHECK(lower.context == ShortcutContext::global);
    CHECK(irama_studio::chords_text(lower) == "Numpad /");
    const Shortcut& raise = irama_studio::find_shortcut(ShortcutId::raise_octave);
    CHECK(raise.context == ShortcutContext::global);
    CHECK(irama_studio::chords_text(raise) == "Numpad *");
}
