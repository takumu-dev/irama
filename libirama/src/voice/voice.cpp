// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include <irama/engine/voice/voice.hpp>

#include <irama/engine/base/assertion.hpp>
#include <irama/engine/base/narrow.hpp>
#include <irama/engine/base/nonblocking.hpp>
#include <irama/engine/base/pcg32.hpp>
#include <irama/engine/source/tuning.hpp>
#include <irama/engine/voice/adsr.hpp>
#include <irama/engine/voice/instrument.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <ranges>
#include <span>

namespace irama {

void Voice::start(int note, float velocity, const Instrument& instrument, const Pcg32& noise) {
    IRAMA_ASSERT(velocity >= 0.0F && velocity <= 1.0F, "a velocity is from 0 to 1");
    const float sample_rate = instrument.sample_rate();
    frequency_ = note_frequency(note);
    oscillator_.start(sample_rate);
    waveform_ = instrument.waveform();
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
    const auto wave = scratch.first(frames);
    const auto other = scratch.subspan(frames, frames);
    // The frequencies, then the envelope's levels.
    const auto third = scratch.subspan(2 * frames, frames);
    render_source(wave, third, other, block);
    envelope_.render(third, block.sustain, block.envelope);
    auto sample = out.begin();
    for (const auto [wave_sample, level, left, right] :
         std::views::zip(wave, third, block.left_gain, block.right_gain)) {
        const float value = wave_sample * level * velocity_gain_;
        *sample++ += value * left;
        *sample++ += value * right;
    }
}

void Voice::render_source(std::span<float> wave, std::span<float> frequencies,
                          std::span<float> other, const InstrumentBlock& block) IRAMA_NONBLOCKING {
    // A stepped value changes only where a block starts, so a change is seen on its own frame.
    if (block.waveform != waveform_) {
        fading_ = oscillator_;
        fading_waveform_ = waveform_;
        waveform_ = block.waveform;
        crossfade_position_ = 0;
    }

    for (const auto [frequency, tuning] : std::views::zip(frequencies, block.tuning)) {
        frequency = frequency_ * tuning;
    }
    oscillator_.render(wave, waveform_, frequencies, block.pulse_width);
    if (crossfade_position_ < crossfade_frames_) {
        fading_.render(other, fading_waveform_, frequencies, block.pulse_width);
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

    noise_.render(other);
    for (const auto [sample, noise, osc_level, noise_level] :
         std::views::zip(wave, other, block.osc_level, block.noise_level)) {
        sample = (sample * osc_level) + (noise * noise_level);
    }
}

bool Voice::active() const { return envelope_.active(); }

bool Voice::held() const { return envelope_.active() && !envelope_.releasing(); }

int Voice::note() const { return note_; }

} // namespace irama
