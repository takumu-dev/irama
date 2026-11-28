// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include "audio/instrument_values.hpp"

#include "audio/create_player.hpp"
#include <irama/engine/base/allocator.hpp>
#include <irama/engine/model/change_set.hpp>
#include <irama/engine/model/instrument_part.hpp>
#include <irama/engine/model/song_part.hpp>
#include <irama/engine/param/param_descriptor.hpp>
#include <irama/engine/param/param_id.hpp>
#include <irama/engine/player/player.hpp>
#include <irama/engine/voice/instrument.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <ranges>
#include <utility>
#include <vector>

using irama::InstrumentParam;
using irama::ParamDescriptor;
using irama::ParamId;
using irama_studio::create_player;
using irama_studio::InstrumentValues;

namespace {

[[nodiscard]] irama::PlayerConfig config() { return {.sample_rate = 48'000, .max_voices = 8}; }

// One value changed.
struct Change {
    ParamId id{};
    float value = 0.0F;
};

// Changes to parameters from every group, several of them heard while a note sounds.
const std::array changes{
    Change{.id = InstrumentParam::osc1_waveform, .value = 3.0F},
    Change{.id = InstrumentParam::gain, .value = -3.0F},
    Change{.id = InstrumentParam::cutoff, .value = 2'000.0F},
    Change{.id = InstrumentParam::pan, .value = -0.5F},
    Change{.id = InstrumentParam::filter_mode, .value = 1.0F},
    Change{.id = InstrumentParam::attack, .value = 0.05F},
    Change{.id = InstrumentParam::osc1_pulse_width, .value = 25.0F},
    Change{.id = InstrumentParam::sustain, .value = 40.0F},
};

[[nodiscard]] InstrumentValues changed_values() {
    InstrumentValues values{irama::instrument_params};
    for (const Change& change : changes) {
        values.set(change.id, change.value);
    }
    return values;
}

[[nodiscard]] irama::PlayerPtr make_player(const InstrumentValues& values) {
    auto player = create_player(config(), values);
    REQUIRE(player.has_value());
    return std::move(*player);
}

// A player given the changed instrument directly, at frame 0, rather than through Studio's copy.
[[nodiscard]] irama::PlayerPtr make_changed_player() {
    std::array<float, irama::instrument_params.size()> values{};
    std::ranges::transform(irama::instrument_params, values.begin(),
                           &ParamDescriptor::default_value);
    for (const Change& change : changes) {
        const std::size_t index = irama::find_param(irama::instrument_params, change.id)
                                      .value_or(irama::instrument_params.size());
        values.at(index) = change.value;
    }
    auto part = irama::InstrumentPart::create(irama::default_allocator(), values);
    auto changes_set = irama::ChangeSet::create(irama::default_allocator(), {.instruments = 1});
    auto player = irama::Player::create(config());
    REQUIRE(part.has_value());
    REQUIRE(changes_set.has_value());
    REQUIRE(player.has_value());
    (*changes_set)->replace_instrument(1, std::move(*part));
    REQUIRE((*player)->change(0, *changes_set));
    return std::move(*player);
}

// Plays a chord on instrument 01 from frame 0, and renders the first 0.5 s of it, interleaved
// stereo.
[[nodiscard]] std::vector<float> play_chord(irama::Player& player) {
    for (const int note : {48, 52, 55}) {
        REQUIRE(player.note_on(0, note, 1.0F, 1));
    }
    std::vector<float> out(std::size_t{24'000} * 2);
    player.render(out);
    return out;
}

} // namespace

TEST_CASE("Studio's copy starts at the defaults", "[studio][values]") {
    const InstrumentValues values{irama::instrument_params};
    for (const ParamDescriptor& param : irama::instrument_params) {
        CHECK(values.value(param.id) == param.default_value);
    }
}

TEST_CASE("a value set in the copy is constrained to its parameter", "[studio][values]") {
    InstrumentValues values{irama::instrument_params};
    values.set(InstrumentParam::gain, 100.0F);
    values.set(InstrumentParam::osc1_coarse, 6.6F);
    values.set(InstrumentParam::sustain, -std::numeric_limits<float>::infinity());
    CHECK(values.value(InstrumentParam::gain) == 12.0F);
    CHECK(values.value(InstrumentParam::osc1_coarse) == 7.0F);
    CHECK(values.value(InstrumentParam::sustain) == 0.0F);
}

TEST_CASE("the copy's values are in the order of the table", "[studio][values]") {
    const InstrumentValues values = changed_values();
    REQUIRE(values.values().size() == irama::instrument_params.size());
    for (const auto [param, value] : std::views::zip(irama::instrument_params, values.values())) {
        CHECK(value == values.value(param.id));
    }
}

TEST_CASE("an instrument change holds the copy as instrument 01, and the song change the new "
          "song's song part too",
          "[studio][values]") {
    const InstrumentValues values = changed_values();
    const irama::ChangeSetPtr instrument = irama_studio::instrument_change(values);
    CHECK_FALSE(instrument->replaces_song());
    REQUIRE(instrument->instruments().size() == 1);
    CHECK(instrument->instruments().front().number == irama_studio::studio_instrument);
    CHECK(std::ranges::equal(instrument->instruments().front().part->values(), values.values()));

    const irama::ChangeSetPtr song = irama_studio::song_change(values);
    REQUIRE(song->replaces_song());
    const auto new_song = irama::SongPart::create(irama::default_allocator());
    REQUIRE(new_song.has_value());
    CHECK(*song->song() == **new_song);
    REQUIRE(song->instruments().size() == 1);
    CHECK(std::ranges::equal(song->instruments().front().part->values(), values.values()));
}

TEST_CASE("a new player starts with Studio's copy, so it holds every changed value at its first "
          "frame",
          "[studio][values]") {
    const irama::PlayerPtr reopened = make_player(changed_values());
    const irama::PlayerPtr direct = make_changed_player();
    const irama::PlayerPtr untouched = make_player(InstrumentValues{irama::instrument_params});

    const std::vector<float> played = play_chord(*reopened);
    CHECK(played == play_chord(*direct));
    CHECK(played != play_chord(*untouched));
}
