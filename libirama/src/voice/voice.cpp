// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include <irama/engine/voice/voice.hpp>

#include <irama/engine/base/assertion.hpp>
#include <irama/engine/base/nonblocking.hpp>
#include <irama/engine/source/tuning.hpp>
#include <irama/engine/voice/adsr.hpp>

#include <cmath>
#include <cstddef>
#include <numbers>
#include <ranges>
#include <span>

namespace {

// The amplifier's gain, −12 dB, so that a few voices together stay under full scale.
constexpr double amplifier_gain_db = -12.0;

// The gain of each channel of a voice panned to the centre, at a velocity of 1: the amplifier's
// gain, and cos(π/4) for the constant-power pan, which is the same on both sides.
[[nodiscard]] float centre_gain() {
    return static_cast<float>(std::pow(10.0, amplifier_gain_db / 20.0) *
                              std::cos(std::numbers::pi / 4.0));
}

// The amplifier envelope, at its default settings.
constexpr irama::AdsrSettings amplifier_envelope;

} // namespace

namespace irama {

void Voice::start(int note, float velocity, float sample_rate) {
    IRAMA_ASSERT(velocity >= 0.0F && velocity <= 1.0F, "a velocity is from 0 to 1");
    oscillator_.start(note_frequency(note), sample_rate);
    envelope_.start(amplifier_envelope, sample_rate);
    // The velocity's gain at full velocity sensitivity.
    gain_ = centre_gain() * velocity * velocity;
    note_ = note;
}

void Voice::release() { envelope_.release(); }

void Voice::render(std::span<float> out, std::span<float> scratch) IRAMA_NONBLOCKING {
    if (!envelope_.active()) {
        return;
    }
    IRAMA_ASSERT(out.size() % 2 == 0, "stereo frames have two samples each");
    IRAMA_ASSERT(scratch.size() >= out.size(), "a voice has room for its working");
    const std::size_t frames = out.size() / 2;
    const auto wave = scratch.first(frames);
    const auto levels = scratch.subspan(frames, frames);
    oscillator_.render(wave);
    envelope_.render(levels);
    auto sample = out.begin();
    for (const auto [wave_sample, level] : std::views::zip(wave, levels)) {
        const float value = wave_sample * level * gain_;
        *sample++ += value;
        *sample++ += value;
    }
}

bool Voice::active() const { return envelope_.active(); }

bool Voice::held() const { return envelope_.active() && !envelope_.releasing(); }

int Voice::note() const { return note_; }

} // namespace irama
