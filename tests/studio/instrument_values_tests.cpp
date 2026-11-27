// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include "audio/instrument_values.hpp"

#include "audio/create_player.hpp"
#include <irama/engine/param/param_descriptor.hpp>
#include <irama/engine/param/param_id.hpp>
#include <irama/engine/player/player.hpp>
#include <irama/engine/voice/instrument.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <limits>
#include <span>
#include <utility>
#include <vector>

using irama::InstrumentParam;
using irama::ParamDescriptor;
using irama::ParamId;
using irama_studio::create_player;
using irama_studio::InstrumentValues;

namespace {

[[nodiscard]] irama::PlayerConfig config() { return {.sample_rate = 48'000, .max_voices = 8}; }

// One value sent to a player.
struct Sent {
    ParamId id{};
    float value = 0.0F;

    friend bool operator==(const Sent&, const Sent&) = default;
};

// Stands in for a player's queue, which takes values until it is full.
class Queue {
  public:
    explicit Queue(std::size_t room = std::numeric_limits<std::size_t>::max()) : room_(room) {}

    [[nodiscard]] auto pusher() {
        return [this](ParamId id, float value) {
            if (taken_.size() == room_) {
                return false;
            }
            taken_.push_back({.id = id, .value = value});
            return true;
        };
    }

    // Empties the queue, as a render call would.
    [[nodiscard]] std::vector<Sent> drain() { return std::exchange(taken_, {}); }

  private:
    std::size_t room_;
    std::vector<Sent> taken_;
};

// Changes to parameters from every group, several of them heard while a note sounds.
const std::array changes{
    Sent{.id = InstrumentParam::osc1_waveform, .value = 3.0F},
    Sent{.id = InstrumentParam::gain, .value = -3.0F},
    Sent{.id = InstrumentParam::cutoff, .value = 2'000.0F},
    Sent{.id = InstrumentParam::pan, .value = -0.5F},
    Sent{.id = InstrumentParam::filter_mode, .value = 1.0F},
    Sent{.id = InstrumentParam::attack, .value = 0.05F},
    Sent{.id = InstrumentParam::osc1_pulse_width, .value = 25.0F},
    Sent{.id = InstrumentParam::sustain, .value = 40.0F},
};

[[nodiscard]] irama::PlayerPtr make_player(InstrumentValues& values) {
    auto player = create_player(config(), values);
    REQUIRE(player.has_value());
    return std::move(*player);
}

// A player given the changes directly, at frame 0, rather than through Studio's copy.
[[nodiscard]] irama::PlayerPtr make_changed_player() {
    auto player = irama::Player::create(config());
    REQUIRE(player.has_value());
    for (const Sent& change : changes) {
        REQUIRE((*player)->set_parameter(0, change.id, change.value));
    }
    return std::move(*player);
}

void set_values(InstrumentValues& values, std::span<const Sent> sent) {
    for (const Sent& change : sent) {
        values.set(change.id, change.value);
    }
}

// Plays a chord from frame 0, and renders the first 0.5 s of it, interleaved stereo.
[[nodiscard]] std::vector<float> play_chord(irama::Player& player) {
    for (const int note : {48, 52, 55}) {
        REQUIRE(player.note_on(0, note, 1.0F));
    }
    std::vector<float> out(std::size_t{24'000} * 2);
    player.render(out);
    return out;
}

} // namespace

TEST_CASE("Studio's copy starts at the defaults, with nothing to send", "[studio][values]") {
    const InstrumentValues values{irama::instrument_params};
    for (const ParamDescriptor& param : irama::instrument_params) {
        CHECK(values.value(param.id) == param.default_value);
    }
    CHECK_FALSE(values.waiting());
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

TEST_CASE("a changed value is sent once, at its latest", "[studio][values]") {
    InstrumentValues values{irama::instrument_params};
    Queue queue;
    values.set(InstrumentParam::gain, -6.0F);
    values.set(InstrumentParam::gain, -9.0F);
    CHECK(values.waiting());
    values.flush(queue.pusher());
    CHECK(queue.drain() == std::vector{Sent{.id = InstrumentParam::gain, .value = -9.0F}});
    CHECK_FALSE(values.waiting());
    values.flush(queue.pusher());
    CHECK(queue.drain().empty());
}

TEST_CASE("a value the queue refuses stays in the copy and goes on a later flush",
          "[studio][values]") {
    InstrumentValues values{irama::instrument_params};
    Queue full{0};
    values.set(InstrumentParam::cutoff, 500.0F);
    values.set(InstrumentParam::pan, 0.25F);
    values.flush(full.pusher());
    CHECK(values.waiting());
    CHECK(values.value(InstrumentParam::cutoff) == 500.0F);

    // A drag goes on while the queue is full, and only where it ended goes.
    values.set(InstrumentParam::cutoff, 800.0F);
    Queue roomy;
    values.flush(roomy.pusher());
    CHECK(roomy.drain() == std::vector{Sent{.id = InstrumentParam::cutoff, .value = 800.0F},
                                       Sent{.id = InstrumentParam::pan, .value = 0.25F}});
    CHECK_FALSE(values.waiting());
}

TEST_CASE("a flush stops at the first value refused, and sends the rest later",
          "[studio][values]") {
    InstrumentValues values{irama::instrument_params};
    values.set(InstrumentParam::cutoff, 500.0F);
    values.set(InstrumentParam::gain, -6.0F);
    values.set(InstrumentParam::attack, 0.1F);
    Queue one{1};
    values.flush(one.pusher());
    CHECK(one.drain() == std::vector{Sent{.id = InstrumentParam::cutoff, .value = 500.0F}});
    values.flush(one.pusher());
    CHECK(one.drain() == std::vector{Sent{.id = InstrumentParam::gain, .value = -6.0F}});
    values.flush(one.pusher());
    CHECK(one.drain() == std::vector{Sent{.id = InstrumentParam::attack, .value = 0.1F}});
    CHECK_FALSE(values.waiting());
}

TEST_CASE("sending all sends every value, in the table's order", "[studio][values]") {
    InstrumentValues values{irama::instrument_params};
    values.set(InstrumentParam::gain, -6.0F);
    Queue queue;
    values.send_all();
    values.flush(queue.pusher());
    std::vector<Sent> expected;
    expected.reserve(irama::instrument_params.size());
    for (const ParamDescriptor& param : irama::instrument_params) {
        expected.push_back({.id = param.id, .value = values.value(param.id)});
    }
    CHECK(queue.drain() == expected);
}

TEST_CASE("a new player starts with Studio's copy, so it holds every changed value at its first "
          "frame",
          "[studio][values]") {
    InstrumentValues values{irama::instrument_params};
    const irama::PlayerPtr old_player = make_player(values);
    CHECK_FALSE(values.waiting());

    // Half the changes reach the old player, and its queue refuses the rest, as when its device
    // has stopped calling back.
    set_values(values, std::span{changes}.first(4));
    values.flush([&](ParamId id, float value) { return old_player->set_parameter(0, id, value); });
    set_values(values, std::span{changes}.last(4));
    values.flush([](ParamId /*id*/, float /*value*/) { return false; });
    REQUIRE(values.waiting());

    const irama::PlayerPtr reopened = make_player(values);
    CHECK_FALSE(values.waiting());

    const irama::PlayerPtr direct = make_changed_player();
    InstrumentValues defaults{irama::instrument_params};
    const irama::PlayerPtr untouched = make_player(defaults);

    const std::vector<float> played = play_chord(*reopened);
    CHECK(played == play_chord(*direct));
    CHECK(played != play_chord(*untouched));
}
