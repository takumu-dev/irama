// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include <irama/engine/voice/adsr.hpp>

#include <irama/engine/base/assertion.hpp>
#include <irama/engine/base/narrow.hpp>
#include <irama/engine/base/nonblocking.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <ranges>
#include <span>

namespace {

// The curve control is scaled to the exponent's `k`, so that a curve of ±1 bends its segment as
// much as an analogue envelope does.
constexpr float curve_scale = 6.0F;

// A segment can't last longer than this, far beyond the longest time an envelope takes, so its
// length in frames always fits.
constexpr float max_segment_seconds = 3'600.0F;

[[nodiscard]] bool is_curve(float curve) { return curve >= -1.0F && curve <= 1.0F; }

[[nodiscard]] bool is_time(float seconds) {
    return seconds > 0.0F && seconds <= max_segment_seconds;
}

// The segment's shape at progress `p`, from 0 to 1, for the curve's `k`. expm1 keeps it exact for
// curves near 0.
[[nodiscard]] float shape(float p, float k) {
    if (k == 0.0F) {
        return p;
    }
    return std::expm1(-k * p) / std::expm1(-k);
}

} // namespace

namespace irama {

void Adsr::start(const AdsrSettings& settings, float sample_rate) {
    IRAMA_ASSERT(sample_rate > 0.0F, "a sample rate is above 0 Hz");
    sample_rate_ = sample_rate;
    level_ = 0.0F;
    begin(Stage::attack, settings.attack_seconds, settings.attack_curve, 1.0F);
}

void Adsr::release(const AdsrSettings& settings) {
    if (stage_ == Stage::ended || stage_ == Stage::release) {
        return;
    }
    begin(Stage::release, settings.release_seconds, settings.release_curve, 0.0F);
}

void Adsr::render(std::span<float> levels, std::span<const float> sustain,
                  const AdsrSettings& settings) IRAMA_NONBLOCKING {
    IRAMA_ASSERT(sustain.size() >= levels.size(), "an envelope has a sustain level for each frame");
    for (const auto [level, sustain_level] : std::views::zip(levels, sustain)) {
        if (stage_ == Stage::sustain) {
            level_ = sustain_level;
        }
        if (stage_ == Stage::ended || stage_ == Stage::sustain) {
            level = level_;
            continue;
        }
        // The decay heads for the sustain level as it is at each frame.
        const float end = stage_ == Stage::decay ? sustain_level : end_level_;
        ++position_;
        if (position_ == length_) {
            // A segment ends at its end level exactly, whatever the rounding of its shape.
            level_ = end;
            if (stage_ == Stage::attack) {
                begin(Stage::decay, settings.decay_seconds, settings.decay_curve, 0.0F);
            } else if (stage_ == Stage::decay) {
                stage_ = Stage::sustain;
            } else {
                stage_ = Stage::ended;
            }
        } else {
            const float p = static_cast<float>(position_) / static_cast<float>(length_);
            level_ = start_level_ + ((end - start_level_) * shape(p, k_));
        }
        level = level_;
    }
}

bool Adsr::active() const { return stage_ != Stage::ended; }

bool Adsr::releasing() const { return stage_ == Stage::release; }

void Adsr::begin(Stage stage, float seconds, float curve, float end) {
    IRAMA_ASSERT(is_time(seconds), "an envelope's times are above 0 and at most an hour");
    IRAMA_ASSERT(is_curve(curve), "a curve is from -1 to +1");
    stage_ = stage;
    start_level_ = level_;
    end_level_ = end;
    k_ = curve_scale * curve;
    length_ = std::max(std::uint32_t{1},
                       narrow<std::uint32_t>(std::llround(static_cast<double>(seconds) *
                                                          static_cast<double>(sample_rate_))));
    position_ = 0;
}

} // namespace irama
