// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

// Breaks a precondition of irama::Player, as its argument names, so that a test can check that the
// player stops the program with its report rather than take the bad value to the audio thread.

#include <irama/engine/player/player.hpp>

#include <irama/engine/base/allocator.hpp>
#include <irama/engine/base/narrow.hpp>
#include <irama/engine/model/change_set.hpp>
#include <irama/engine/model/instrument_part.hpp>
#include <irama/engine/param/param_descriptor.hpp>
#include <irama/engine/voice/instrument.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <limits>
#include <span>
#include <string_view>
#include <utility>

#ifdef _MSC_VER
#include <crtdbg.h>
#endif

namespace {

// The exit status of an argument the program doesn't know.
constexpr int usage_error = 2;

using Case = void (*)(irama::Player& player);

void note_too_high(irama::Player& player) { static_cast<void>(player.note_on(0, 120, 1.0F, 1)); }

void note_negative(irama::Player& player) { static_cast<void>(player.note_off(0, -1, 1)); }

void velocity_too_high(irama::Player& player) { static_cast<void>(player.note_on(0, 57, 1.5F, 1)); }

void velocity_nan(irama::Player& player) {
    static_cast<void>(player.note_on(0, 57, std::numeric_limits<float>::quiet_NaN(), 1));
}

void instrument_zero(irama::Player& player) { static_cast<void>(player.note_on(0, 57, 1.0F, 0)); }

void instrument_too_high(irama::Player& player) { static_cast<void>(player.note_off(0, 57, 0xFF)); }

void no_change_set(irama::Player& player) {
    irama::ChangeSetPtr changes;
    static_cast<void>(player.change(0, changes));
}

// Sends a change set of instrument 01 holding `values`. Out of memory, it returns quietly, which
// fails the test that expects a report.
void send_instrument(irama::Player& player, std::span<const float> values) {
    auto changes = irama::ChangeSet::create(irama::default_allocator(), {.instruments = 1});
    auto part = irama::InstrumentPart::create(irama::default_allocator(), values);
    if (!changes || !part) {
        return;
    }
    (*changes)->replace_instrument(1, std::move(*part));
    static_cast<void>(player.change(0, *changes));
}

void instrument_part_short(irama::Player& player) {
    constexpr std::array values{1.0F, 2.0F};
    send_instrument(player, values);
}

void instrument_value_out_of_range(irama::Player& player) {
    std::array<float, irama::instrument_params.size()> values{};
    std::ranges::transform(irama::instrument_params, values.begin(),
                           &irama::ParamDescriptor::default_value);
    // The gain, above its 12 dB.
    values.at(
        irama::find_param(irama::instrument_params, irama::InstrumentParam::gain).value_or(0)) =
        100.0F;
    send_instrument(player, values);
}

void frames_backwards(irama::Player& player) {
    static_cast<void>(player.note_on(100, 57, 1.0F, 1));
    static_cast<void>(player.note_off(99, 57, 1));
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
    NamedCase{.name = "instrument-zero", .run = instrument_zero},
    NamedCase{.name = "instrument-too-high", .run = instrument_too_high},
    NamedCase{.name = "no-change-set", .run = no_change_set},
    NamedCase{.name = "instrument-part-short", .run = instrument_part_short},
    NamedCase{.name = "instrument-value-out-of-range", .run = instrument_value_out_of_range},
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
#ifdef _DEBUG
    // The debug runtime's assertion and error reports, from _ASSERTE, the standard library's
    // checks, /RTC and the debug heap, go to stderr rather than to a dialog.
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
#endif
#endif
    const std::span arguments{argv, irama::narrow<std::size_t>(argc)};
    return arguments.size() == 2 ? run(arguments.back()) : usage_error;
}
