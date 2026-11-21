// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "script.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace irama_cli {
namespace {

constexpr std::string_view blanks = " \t\r";

constexpr int notes_per_octave = 12;
constexpr int highest_octave = 9;

// Reads the whole of `text` as a number of type T, which from_chars can read.
template <typename T> [[nodiscard]] std::optional<T> parse_whole(std::string_view text) {
    T value{};
    const char* const last = std::to_address(text.end());
    const auto [end, error] = std::from_chars(std::to_address(text.begin()), last, value);
    if (error != std::errc{} || end != last) {
        return std::nullopt;
    }
    return value;
}

// Splits a line into its words, leaving out a comment, which begins with a word that begins with
// `#`. A sharp inside a note, such as C#4, is part of its word.
[[nodiscard]] std::vector<std::string_view> split_words(std::string_view line) {
    std::vector<std::string_view> words;
    while (true) {
        const std::size_t first = line.find_first_not_of(blanks);
        if (first == std::string_view::npos || line.at(first) == '#') {
            return words;
        }
        line.remove_prefix(first);
        const std::size_t end = std::min(line.find_first_of(blanks), line.size());
        words.push_back(line.substr(0, end));
        line.remove_prefix(end);
    }
}

// A note as the note column writes it: its name, `-` or `#`, and its octave, such as C-4 or F#3.
// Returns the reason it can't be read, if it can't.
[[nodiscard]] std::expected<int, std::string> parse_note(std::string_view text) {
    constexpr std::string_view names = "CDEFGAB";
    constexpr std::array<int, 7> semitones{0, 2, 4, 5, 7, 9, 11};
    const std::string not_a_note = std::format("`{}` isn't a note, such as C-4 or F#3", text);
    if (text.size() < 3) {
        return std::unexpected(not_a_note);
    }
    const std::size_t name = names.find(text.at(0));
    const bool sharp = text.at(1) == '#';
    // E and B have no sharp in the note column, which writes them as F and C.
    if (name == std::string_view::npos || (text.at(1) != '-' && !sharp) ||
        (sharp && (text.at(0) == 'E' || text.at(0) == 'B'))) {
        return std::unexpected(not_a_note);
    }
    const auto octave = parse_whole<int>(text.substr(2));
    if (!octave) {
        return std::unexpected(not_a_note);
    }
    if (*octave < 0 || *octave > highest_octave) {
        return std::unexpected(std::format("note {} is out of range, C-0 to B-9", text));
    }
    return (*octave * notes_per_octave) + semitones.at(name) + (sharp ? 1 : 0);
}

[[nodiscard]] std::expected<float, std::string> parse_velocity(std::string_view text) {
    const auto velocity = parse_whole<float>(text);
    if (!velocity || !std::isfinite(*velocity)) {
        return std::unexpected(std::format("`{}` isn't a velocity, a number from 0 to 1", text));
    }
    if (*velocity < 0.0F || *velocity > 1.0F) {
        return std::unexpected(std::format("velocity {} is out of range, 0 to 1", text));
    }
    return *velocity;
}

// Reads the words of one line that holds a command. Returns the reason it can't be read, if it
// can't.
[[nodiscard]] std::expected<ScriptMessage, std::string>
parse_command(const std::vector<std::string_view>& words) {
    if (words.size() < 3 || words.at(0) != "at") {
        return std::unexpected("expected `at <frame> <command>`");
    }
    const auto frame = parse_whole<std::uint64_t>(words.at(1));
    if (!frame) {
        return std::unexpected(
            std::format("`{}` isn't a frame, a whole number from 0", words.at(1)));
    }
    ScriptMessage message{.frame = *frame};
    const std::string_view command = words.at(2);
    const std::size_t arguments = words.size() - 3;
    if (command == "note-on") {
        if (arguments != 2) {
            return std::unexpected("note-on takes a note and a velocity");
        }
        const auto note = parse_note(words.at(3));
        if (!note) {
            return std::unexpected(note.error());
        }
        const auto velocity = parse_velocity(words.at(4));
        if (!velocity) {
            return std::unexpected(velocity.error());
        }
        message.kind = ScriptMessage::Kind::note_on;
        message.note = *note;
        message.velocity = *velocity;
    } else if (command == "note-off") {
        if (arguments != 1) {
            return std::unexpected("note-off takes a note");
        }
        const auto note = parse_note(words.at(3));
        if (!note) {
            return std::unexpected(note.error());
        }
        message.kind = ScriptMessage::Kind::note_off;
        message.note = *note;
    } else if (command == "release-all") {
        if (arguments != 0) {
            return std::unexpected("release-all takes nothing more");
        }
        message.kind = ScriptMessage::Kind::release_all;
    } else {
        return std::unexpected(std::format(
            "unknown command `{}`, expected note-on, note-off or release-all", command));
    }
    return message;
}

} // namespace

std::expected<std::vector<ScriptMessage>, ScriptError> parse_script(std::string_view text) {
    std::vector<ScriptMessage> messages;
    for (std::size_t line = 1; !text.empty(); ++line) {
        const std::size_t end = std::min(text.find('\n'), text.size());
        const auto words = split_words(text.substr(0, end));
        text.remove_prefix(std::min(end + 1, text.size()));
        if (words.empty()) {
            continue;
        }
        auto message = parse_command(words);
        if (!message) {
            return std::unexpected(ScriptError{.line = line, .reason = std::move(message.error())});
        }
        if (!messages.empty() && message->frame < messages.back().frame) {
            return std::unexpected(ScriptError{
                .line = line,
                .reason = std::format("frame {} is before frame {} on an earlier line",
                                      message->frame, messages.back().frame),
            });
        }
        messages.push_back(*message);
    }
    return messages;
}

} // namespace irama_cli
