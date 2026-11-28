// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "audio/instrument_values.hpp"

#include <irama/engine/base/assertion.hpp>
#include <irama/engine/param/param_descriptor.hpp>
#include <irama/engine/param/param_id.hpp>

#include <cmath>
#include <cstddef>
#include <optional>
#include <span>

namespace irama_studio {

InstrumentValues::InstrumentValues(std::span<const irama::ParamDescriptor> table) : table_(table) {
    values_.reserve(table.size());
    for (const irama::ParamDescriptor& param : table) {
        values_.push_back(param.default_value);
    }
}

float InstrumentValues::value(irama::ParamId id) const { return values_.at(index_of(id)); }

void InstrumentValues::set(irama::ParamId id, float value) {
    IRAMA_ASSERT(!std::isnan(value), "a parameter's value is a number");
    const std::size_t index = index_of(id);
    values_.at(index) = table_.subspan(index, 1).front().constrain(value);
}

std::size_t InstrumentValues::index_of(irama::ParamId id) const {
    const std::optional<std::size_t> index = irama::find_param(table_, id);
    IRAMA_ASSERT(index.has_value(), "a parameter's ID is one of the instrument's");
    return *index;
}

} // namespace irama_studio
