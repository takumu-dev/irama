// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

// The play test: note keys through the key map, into the audio engine's player, rendered by the
// real callback on miniaudio's Null backend, a device driven by a timer on miniaudio's own thread.
// So the real-time guard and every sanitizer watch Studio's callback while it plays.

#include "audio/audio_device.hpp"
#include "audio/audio_engine.hpp"
#include "input/note_keys.hpp"
#include <irama/engine/voice/instrument.hpp>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <span>
#include <thread>

using irama_studio::AudioBackend;
using irama_studio::AudioEngine;
using irama_studio::AudioGranted;
using irama_studio::NoteKeys;

namespace {

constexpr std::array null_backend{AudioBackend::null};

// USB HID usage IDs of Z, C and B, which play C-4, E-4 and G-4 at octave 4.
constexpr std::array chord_keys{29, 6, 5};

// Lets the callback render for at least `seconds` of audio, or gives up at a deadline far longer
// than that takes even under a sanitizer. Returns whether it rendered that much.
[[nodiscard]] bool render_for(const AudioEngine& engine, double seconds) {
    const std::optional<AudioGranted>& granted = engine.device().granted();
    if (!granted) {
        return false;
    }
    const double callbacks = seconds * granted->sample_rate / granted->callback_frames;
    const std::uint64_t until = engine.heartbeat() + static_cast<std::uint64_t>(callbacks) + 1;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{20};
    while (engine.heartbeat() < until) {
        if (std::chrono::steady_clock::now() > deadline) {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }
    return true;
}

// Presses the chord's keys, and sends what they play to the engine.
void press_chord(NoteKeys& keys, AudioEngine& engine) {
    for (const int key : chord_keys) {
        const std::optional message = keys.press(key, false);
        CHECK(message.has_value());
        if (message) {
            engine.send(*message);
        }
    }
}

} // namespace

TEST_CASE("the audio callback plays silence on the Null backend with no note held",
          "[studio][play]") {
    AudioEngine engine{null_backend};
    REQUIRE(engine.open());
    REQUIRE(engine.device().granted());
    const AudioGranted granted = engine.device().granted().value_or(AudioGranted{});
    CHECK(granted.backend == AudioBackend::null);
    CHECK(granted.callback_frames == 256);
    REQUIRE(render_for(engine, 0.1));
    CHECK(engine.heartbeat() > 0);
    CHECK(engine.take_output_peak() == 0.0F);
    CHECK(engine.active_voices() == 0);
}

TEST_CASE("held note keys sound through the audio callback on the Null backend", "[studio][play]") {
    AudioEngine engine{null_backend};
    REQUIRE(engine.open());
    NoteKeys keys;
    press_chord(keys, engine);
    REQUIRE(render_for(engine, 0.5));
    const float peak = engine.take_output_peak();
    INFO("peak while held " << peak);
    CHECK(peak > 0.01F);
    CHECK(peak <= 1.0F);
    CHECK(engine.active_voices() == 3);
}

TEST_CASE("note keys go silent after release-all on the Null backend", "[studio][play]") {
    AudioEngine engine{null_backend};
    REQUIRE(engine.open());
    NoteKeys keys;
    press_chord(keys, engine);
    REQUIRE(render_for(engine, 0.2));
    REQUIRE(engine.active_voices() == 3);

    // Each voice's release lasts 0.2 s at the default settings.
    engine.send(keys.release_all());
    REQUIRE(render_for(engine, 0.4));
    CHECK(engine.active_voices() == 0);
    // The releases' tails are past, so what plays from here on is silence.
    static_cast<void>(engine.take_output_peak());
    REQUIRE(render_for(engine, 0.05));
    CHECK(engine.take_output_peak() == 0.0F);

    engine.close();
    CHECK_FALSE(engine.is_open());
}

TEST_CASE("note keys do nothing while no device is open", "[studio][play]") {
    AudioEngine engine{null_backend};
    REQUIRE_FALSE(engine.is_open());
    NoteKeys keys;
    press_chord(keys, engine);
    engine.flush();
    CHECK(engine.active_voices() == 0);
    CHECK(engine.heartbeat() == 0);
}

TEST_CASE("with no working backend the engine stays silent and says why", "[studio][play]") {
    AudioEngine engine{std::span<const AudioBackend>{}};
    CHECK_FALSE(engine.open());
    CHECK_FALSE(engine.is_open());
    CHECK_FALSE(engine.last_error().empty());
}

TEST_CASE("a parameter changed while a device plays reaches its player at once", "[studio][play]") {
    AudioEngine engine{null_backend};
    REQUIRE(engine.open());
    NoteKeys keys;
    press_chord(keys, engine);
    REQUIRE(render_for(engine, 0.2));
    const float loud = engine.take_output_peak();
    REQUIRE(loud > 0.01F);

    // 60 dB below the default gain of -12 dB is far quieter, once the 20 ms glide has passed.
    engine.set_parameter(irama::InstrumentParam::gain, -60.0F);
    CHECK(engine.parameters().value(irama::InstrumentParam::gain) == -60.0F);
    REQUIRE(render_for(engine, 0.05));
    static_cast<void>(engine.take_output_peak());
    REQUIRE(render_for(engine, 0.1));
    const float quiet = engine.take_output_peak();
    INFO("peak at the default gain " << loud << ", and at -60 dB " << quiet);
    CHECK(quiet > 0.0F);
    CHECK(quiet < loud / 100.0F);
}

TEST_CASE("a parameter changed with no device open changes only the copy, and the next device's "
          "player starts with it",
          "[studio][play]") {
    AudioEngine engine{null_backend};
    engine.set_parameter(irama::InstrumentParam::gain, -60.0F);
    engine.flush();
    CHECK(engine.parameters().value(irama::InstrumentParam::gain) == -60.0F);

    REQUIRE(engine.open());
    NoteKeys keys;
    press_chord(keys, engine);
    REQUIRE(render_for(engine, 0.2));
    const float quiet = engine.take_output_peak();
    CHECK(quiet > 0.0F);
    CHECK(quiet < 0.01F);

    // Reopening gives the device a new player, which starts with the copy too. The new player
    // starts silent, so the keys are pressed again.
    engine.close();
    REQUIRE(engine.open());
    NoteKeys pressed_again;
    press_chord(pressed_again, engine);
    REQUIRE(render_for(engine, 0.2));
    const float reopened = engine.take_output_peak();
    CHECK(reopened > 0.0F);
    CHECK(reopened < 0.01F);
}
