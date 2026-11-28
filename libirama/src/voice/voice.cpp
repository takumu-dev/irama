// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include <irama/engine/voice/voice.hpp>

#include <irama/engine/base/assertion.hpp>
#include <irama/engine/base/narrow.hpp>
#include <irama/engine/base/nonblocking.hpp>
#include <irama/engine/base/pcg32.hpp>
#include <irama/engine/filter/svf.hpp>
#include <irama/engine/source/oscillator.hpp>
#include <irama/engine/source/tuning.hpp>
#include <irama/engine/voice/adsr.hpp>
#include <irama/engine/voice/instrument.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <ranges>
#include <span>
#include <tuple>

namespace {

// Mixes the oscillator's samples in `wave` with the noise, at their levels of each frame.
void mix_noise(std::span<float> wave, std::span<const float> noise,
               const irama::InstrumentBlock& block) IRAMA_NONBLOCKING {
    for (const auto [sample, noise_sample, osc_level, noise_level] :
         std::views::zip(wave, noise, block.osc_level, block.noise_level)) {
        sample = (sample * osc_level) + (noise_sample * noise_level);
    }
}

} // namespace

namespace irama {

namespace {

// A length in seconds as a whole number of frames, at least one.
[[nodiscard]] std::uint32_t frames_of(float seconds, float sample_rate) {
    return std::max<std::uint32_t>(
        1, irama::narrow<std::uint32_t>(std::lround(seconds * sample_rate)));
}

} // namespace

void Voice::start(int note, float velocity, const Instrument& instrument, const Pcg32& noise,
                  std::uint64_t order) {
    IRAMA_ASSERT(velocity >= 0.0F && velocity <= 1.0F, "a velocity is from 0 to 1");
    const float sample_rate = instrument.sample_rate();
    frequency_ = note_frequency(note);
    chain_.oscillator.start(sample_rate);
    chain_.waveform = instrument.waveform();
    chain_.filter.reset();
    chain_.filter_mode = instrument.filter_mode();
    chain_.filter_slope = instrument.filter_slope();
    crossfade_frames_ = frames_of(crossfade_seconds, sample_rate);
    crossfade_position_ = crossfade_frames_;
    fade_frames_ = frames_of(steal_fade_seconds, sample_rate);
    stolen_ = false;
    fade_position_ = 0;
    noise_.start(noise);
    envelope_.start(instrument.envelope(), sample_rate);
    velocity_gain_ = instrument.velocity_gain(velocity);
    pan_fixed_ = false;
    order_ = order;
    note_ = note;
}

void Voice::fix_pan(float position) {
    pan_fixed_ = true;
    std::tie(left_pan_, right_pan_) = pan_gains(position);
}

void Voice::release(const Instrument& instrument) { envelope_.release(instrument.envelope()); }

void Voice::fade_out() {
    IRAMA_DEBUG_ASSERT(active() && !stolen_, "only a sounding voice that isn't fading can fade");
    stolen_ = true;
    fade_position_ = 0;
}

void Voice::render(std::span<float> out, std::span<float> scratch,
                   const InstrumentBlock& block) IRAMA_NONBLOCKING {
    if (!active()) {
        return;
    }
    IRAMA_ASSERT(out.size() % 2 == 0, "stereo frames have two samples each");
    const std::size_t frames = out.size() / 2;
    IRAMA_ASSERT(scratch.size() >= frames * scratch_per_frame, "a voice has room for its working");
    IRAMA_ASSERT(block.left_gain.size() >= frames && block.right_gain.size() >= frames,
                 "a voice has the instrument's values for each frame");
    IRAMA_ASSERT(block.filter.size() >= frames, "a voice has the filter's values for each frame");
    IRAMA_ASSERT(!pan_fixed_ || block.gain.size() >= frames,
                 "a voice with a pan of its own has the instrument's gain for each frame");
    const auto wave = scratch.first(frames);
    const auto other = scratch.subspan(frames, frames);
    const auto third = scratch.subspan(2 * frames, frames);
    render_chains(wave, other, third, block);
    // The envelope's levels.
    envelope_.render(third, block.sustain, block.envelope);
    if (stolen_) {
        // Frame i of the fade is at 1 − (i + 1) / n of the level, and every frame after it at 0.
        const auto length = static_cast<float>(fade_frames_);
        for (float& level : third) {
            fade_position_ = std::min(fade_position_ + 1, fade_frames_);
            level *= 1.0F - (static_cast<float>(fade_position_) / length);
        }
    }
    auto sample = out.begin();
    if (pan_fixed_) {
        for (const auto [wave_sample, level, linear] : std::views::zip(wave, third, block.gain)) {
            const float value = wave_sample * level * velocity_gain_;
            *sample++ += value * (linear * left_pan_);
            *sample++ += value * (linear * right_pan_);
        }
        return;
    }
    for (const auto [wave_sample, level, left, right] :
         std::views::zip(wave, third, block.left_gain, block.right_gain)) {
        const float value = wave_sample * level * velocity_gain_;
        *sample++ += value * left;
        *sample++ += value * right;
    }
}

void Voice::render_chains(std::span<float> wave, std::span<float> other, std::span<float> third,
                          const InstrumentBlock& block) IRAMA_NONBLOCKING {
    // A stepped value changes only where a block starts, so a change is seen on its own frame.
    if (block.waveform != chain_.waveform || block.filter_mode != chain_.filter_mode ||
        block.filter_slope != chain_.filter_slope) {
        fading_ = chain_;
        chain_.waveform = block.waveform;
        chain_.filter_mode = block.filter_mode;
        chain_.filter_slope = block.filter_slope;
        crossfade_position_ = 0;
    }
    const bool crossfading = crossfade_position_ < crossfade_frames_;

    // The frequencies, which both oscillators play, then the noise, which both filters hear.
    const auto frequencies = third;
    for (const auto [frequency, tuning] : std::views::zip(frequencies, block.tuning)) {
        frequency = frequency_ * tuning;
    }
    chain_.oscillator.render(wave, chain_.waveform, frequencies, block.pulse_width);
    if (crossfading) {
        fading_.oscillator.render(other, fading_.waveform, frequencies, block.pulse_width);
    }
    const auto noise = third;
    noise_.render(noise);

    mix_noise(wave, noise, block);
    chain_.filter.render(wave, chain_.filter_mode, chain_.filter_slope, block.filter);
    if (!crossfading) {
        return;
    }
    mix_noise(other, noise, block);
    fading_.filter.render(other, fading_.filter_mode, fading_.filter_slope, block.filter);
    const auto length = static_cast<float>(crossfade_frames_);
    for (const auto [new_sample, old_sample] : std::views::zip(wave, other)) {
        if (crossfade_position_ == crossfade_frames_) {
            break;
        }
        ++crossfade_position_;
        const float progress = static_cast<float>(crossfade_position_) / length;
        new_sample = ((1.0F - progress) * old_sample) + (progress * new_sample);
    }
}

bool Voice::active() const {
    return envelope_.active() && (!stolen_ || fade_position_ < fade_frames_);
}

bool Voice::held() const { return active() && !stolen_ && !envelope_.releasing(); }

bool Voice::releasing() const { return active() && envelope_.releasing(); }

bool Voice::fading() const { return active() && stolen_; }

std::uint32_t Voice::fade_frames_left() const { return fade_frames_ - fade_position_; }

float Voice::level() const { return envelope_.level() * velocity_gain_; }

std::uint64_t Voice::order() const { return order_; }

int Voice::note() const { return note_; }

} // namespace irama
