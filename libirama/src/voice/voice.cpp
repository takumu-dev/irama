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

void Voice::start(int note, float velocity, const Instrument& instrument, const Pcg32& noise) {
    IRAMA_ASSERT(velocity >= 0.0F && velocity <= 1.0F, "a velocity is from 0 to 1");
    const float sample_rate = instrument.sample_rate();
    frequency_ = note_frequency(note);
    chain_.oscillator.start(sample_rate);
    chain_.waveform = instrument.waveform();
    chain_.filter.reset();
    chain_.filter_mode = instrument.filter_mode();
    chain_.filter_slope = instrument.filter_slope();
    crossfade_frames_ = std::max<std::uint32_t>(
        1, narrow<std::uint32_t>(std::lround(crossfade_seconds * sample_rate)));
    crossfade_position_ = crossfade_frames_;
    noise_.start(noise);
    envelope_.start(instrument.envelope(), sample_rate);
    velocity_gain_ = instrument.velocity_gain(velocity);
    note_ = note;
}

void Voice::release(const Instrument& instrument) { envelope_.release(instrument.envelope()); }

void Voice::render(std::span<float> out, std::span<float> scratch,
                   const InstrumentBlock& block) IRAMA_NONBLOCKING {
    if (!envelope_.active()) {
        return;
    }
    IRAMA_ASSERT(out.size() % 2 == 0, "stereo frames have two samples each");
    const std::size_t frames = out.size() / 2;
    IRAMA_ASSERT(scratch.size() >= frames * scratch_per_frame, "a voice has room for its working");
    IRAMA_ASSERT(block.left_gain.size() >= frames && block.right_gain.size() >= frames,
                 "a voice has the instrument's values for each frame");
    IRAMA_ASSERT(block.filter.size() >= frames, "a voice has the filter's values for each frame");
    const auto wave = scratch.first(frames);
    const auto other = scratch.subspan(frames, frames);
    const auto third = scratch.subspan(2 * frames, frames);
    render_chains(wave, other, third, block);
    // The envelope's levels.
    envelope_.render(third, block.sustain, block.envelope);
    auto sample = out.begin();
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

bool Voice::active() const { return envelope_.active(); }

bool Voice::held() const { return envelope_.active() && !envelope_.releasing(); }

int Voice::note() const { return note_; }

} // namespace irama
