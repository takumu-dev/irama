// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include <irama/engine/voice/voice.hpp>

#include <irama/engine/base/assertion.hpp>
#include <irama/engine/base/nonblocking.hpp>
#include <irama/engine/source/tuning.hpp>
#include <irama/engine/voice/adsr.hpp>
#include <irama/engine/voice/instrument.hpp>

#include <cstddef>
#include <ranges>
#include <span>

namespace irama {

void Voice::start(int note, float velocity, const Instrument& instrument) {
    IRAMA_ASSERT(velocity >= 0.0F && velocity <= 1.0F, "a velocity is from 0 to 1");
    oscillator_.start(note_frequency(note), instrument.sample_rate());
    envelope_.start(instrument.envelope(), instrument.sample_rate());
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
    IRAMA_ASSERT(scratch.size() >= out.size(), "a voice has room for its working");
    const std::size_t frames = out.size() / 2;
    IRAMA_ASSERT(block.left_gain.size() >= frames && block.right_gain.size() >= frames,
                 "a voice has the instrument's values for each frame");
    const auto wave = scratch.first(frames);
    const auto levels = scratch.subspan(frames, frames);
    oscillator_.render(wave);
    envelope_.render(levels, block.sustain, block.envelope);
    auto sample = out.begin();
    for (const auto [wave_sample, level, left, right] :
         std::views::zip(wave, levels, block.left_gain, block.right_gain)) {
        const float value = wave_sample * level * velocity_gain_;
        *sample++ += value * left;
        *sample++ += value * right;
    }
}

bool Voice::active() const { return envelope_.active(); }

bool Voice::held() const { return envelope_.active() && !envelope_.releasing(); }

int Voice::note() const { return note_; }

} // namespace irama
