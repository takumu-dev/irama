// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

// Breaks a precondition of irama::Player, as its argument names, so that a test can check that the
// player stops the program with its report rather than take the bad value to the audio thread.

#include <irama/engine/player/player.hpp>

#include <irama/engine/base/narrow.hpp>
#include <irama/engine/param/param_id.hpp>
#include <irama/engine/voice/instrument.hpp>

#include <array>
#include <cstddef>
#include <cstdlib>
#include <limits>
#include <span>
#include <string_view>

namespace {

// The exit status of an argument the program doesn't know.
constexpr int usage_error = 2;

using Case = void (*)(irama::Player& player);

void note_too_high(irama::Player& player) { static_cast<void>(player.note_on(0, 120, 1.0F)); }

void note_negative(irama::Player& player) { static_cast<void>(player.note_off(0, -1)); }

void velocity_too_high(irama::Player& player) { static_cast<void>(player.note_on(0, 57, 1.5F)); }

void velocity_nan(irama::Player& player) {
    static_cast<void>(player.note_on(0, 57, std::numeric_limits<float>::quiet_NaN()));
}

void unknown_parameter(irama::Player& player) {
    static_cast<void>(player.set_parameter(0, irama::ParamId{4'242}, 1.0F));
}

void parameter_nan(irama::Player& player) {
    static_cast<void>(player.set_parameter(0, irama::InstrumentParam::gain,
                                           std::numeric_limits<float>::quiet_NaN()));
}

void frames_backwards(irama::Player& player) {
    static_cast<void>(player.note_on(100, 57, 1.0F));
    static_cast<void>(player.note_off(99, 57));
}

void odd_length(irama::Player& player) {
    std::array<float, 3> out{};
    player.render(out);
}

// A second player, beside the one each case is given.
void too_many_voices(irama::Player& /*player*/) {
    [[maybe_unused]] const auto created =
        irama::Player::create({.max_voices = irama::Player::max_voices_limit + 1});
}

void no_voices(irama::Player& /*player*/) {
    [[maybe_unused]] const auto created = irama::Player::create({.max_voices = 0});
}

struct NamedCase {
    std::string_view name;
    Case run = nullptr;
};

constexpr std::array cases{
    NamedCase{.name = "note-too-high", .run = note_too_high},
    NamedCase{.name = "note-negative", .run = note_negative},
    NamedCase{.name = "velocity-too-high", .run = velocity_too_high},
    NamedCase{.name = "velocity-nan", .run = velocity_nan},
    NamedCase{.name = "unknown-parameter", .run = unknown_parameter},
    NamedCase{.name = "parameter-nan", .run = parameter_nan},
    NamedCase{.name = "frames-backwards", .run = frames_backwards},
    NamedCase{.name = "odd-length", .run = odd_length},
    NamedCase{.name = "too-many-voices", .run = too_many_voices},
    NamedCase{.name = "no-voices", .run = no_voices},
};

int run(std::string_view name) {
    for (const auto& named : cases) {
        if (named.name == name) {
            auto player = irama::Player::create({});
            if (!player) {
                return EXIT_FAILURE;
            }
            named.run(**player);
            return EXIT_SUCCESS;
        }
    }
    return usage_error;
}

} // namespace

int main(int argc, char** argv) {
#ifdef _MSC_VER
    // MSVC's runtime answers abort() with a dialog in debug builds, and with a Windows Error
    // Reporting fault report, either of which can wait for a click that never comes in CI.
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    const std::span arguments{argv, irama::narrow<std::size_t>(argc)};
    return arguments.size() == 2 ? run(arguments.back()) : usage_error;
}
