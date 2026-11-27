// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include <irama/engine/param/param_descriptor.hpp>

#include <algorithm>
#include <cmath>

namespace irama {

float ParamDescriptor::to_normalized(float value) const {
    if (scale == ParamScale::logarithmic) {
        return std::log(value / min) / std::log(max / min);
    }
    return (value - min) / (max - min);
}

float ParamDescriptor::from_normalized(float normalized) const {
    if (scale == ParamScale::logarithmic) {
        return min * std::exp(normalized * std::log(max / min));
    }
    return min + (normalized * (max - min));
}

float ParamDescriptor::constrain(float value) const {
    const float clamped = std::clamp(value, min, max);
    return scale == ParamScale::stepped ? std::round(clamped) : clamped;
}

} // namespace irama
