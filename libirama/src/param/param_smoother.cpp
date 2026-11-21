// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include <irama/engine/param/param_smoother.hpp>

#include <irama/engine/base/assertion.hpp>
#include <irama/engine/base/narrow.hpp>
#include <irama/engine/base/nonblocking.hpp>
#include <irama/engine/param/param_descriptor.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>

namespace {

// The frames a ramp takes: the descriptor's time, rounded, and at least one if it has a time.
[[nodiscard]] std::uint32_t ramp_frames(const irama::ParamDescriptor& descriptor,
                                        float sample_rate) {
    if (descriptor.smoothing_seconds == 0.0F) {
        return 0;
    }
    const auto frames = std::llround(static_cast<double>(descriptor.smoothing_seconds) *
                                     static_cast<double>(sample_rate));
    return std::max(std::uint32_t{1}, irama::narrow<std::uint32_t>(frames));
}

} // namespace

namespace irama {

ParamSmoother::ParamSmoother(const ParamDescriptor& descriptor, float sample_rate)
    : descriptor_{&descriptor}, ramp_frames_{ramp_frames(descriptor, sample_rate)},
      value_{descriptor.default_value}, target_{descriptor.default_value} {
    IRAMA_ASSERT(sample_rate > 0.0F, "a sample rate is above 0 Hz");
}

void ParamSmoother::set(float value, bool smooth) IRAMA_NONBLOCKING {
    IRAMA_DEBUG_ASSERT(value == descriptor_->constrain(value),
                       "a parameter's value is constrained to it before it is set");
    target_ = value;
    if (!smooth || ramp_frames_ == 0) {
        value_ = value;
        length_ = 0;
        position_ = 0;
        return;
    }
    start_ = descriptor_->to_normalized(value_);
    end_ = descriptor_->to_normalized(value);
    length_ = ramp_frames_;
    position_ = 0;
}

void ParamSmoother::render(std::span<float> values) const IRAMA_NONBLOCKING {
    std::uint32_t position = position_;
    for (float& value : values) {
        // Past the ramp's end, or at rest, the value is the target.
        if (position == length_) {
            value = target_;
            continue;
        }
        ++position;
        value = at(position);
    }
}

void ParamSmoother::advance(std::size_t frames) IRAMA_NONBLOCKING {
    if (position_ == length_) {
        return;
    }
    position_ = narrow<std::uint32_t>(std::min<std::size_t>(position_ + frames, length_));
    value_ = at(position_);
}

float ParamSmoother::value() const { return value_; }

float ParamSmoother::target() const { return target_; }

bool ParamSmoother::ramping() const { return position_ != length_; }

float ParamSmoother::at(std::uint32_t position) const {
    // A ramp ends at its target exactly, whatever the rounding on the way.
    if (position == length_) {
        return target_;
    }
    const float progress = static_cast<float>(position) / static_cast<float>(length_);
    return descriptor_->from_normalized(start_ + ((end_ - start_) * progress));
}

} // namespace irama
