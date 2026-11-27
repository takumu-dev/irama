// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "audio/instrument_values.hpp"

#include <irama/engine/base/assertion.hpp>
#include <irama/engine/param/param_descriptor.hpp>
#include <irama/engine/param/param_id.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <span>

namespace irama_studio {

InstrumentValues::InstrumentValues(std::span<const irama::ParamDescriptor> table) : table_(table) {
    entries_.reserve(table.size());
    for (const irama::ParamDescriptor& param : table) {
        entries_.push_back({.value = param.default_value, .waiting = false});
    }
}

float InstrumentValues::value(irama::ParamId id) const { return entries_.at(index_of(id)).value; }

void InstrumentValues::set(irama::ParamId id, float value) {
    IRAMA_ASSERT(!std::isnan(value), "a parameter's value is a number");
    const std::size_t index = index_of(id);
    entries_.at(index) = {
        .value = table_.subspan(index, 1).front().constrain(value),
        .waiting = true,
    };
}

void InstrumentValues::send_all() {
    for (Entry& entry : entries_) {
        entry.waiting = true;
    }
}

bool InstrumentValues::waiting() const {
    return std::ranges::any_of(entries_, [](const Entry& entry) { return entry.waiting; });
}

std::size_t InstrumentValues::index_of(irama::ParamId id) const {
    const std::optional<std::size_t> index = irama::find_param(table_, id);
    IRAMA_ASSERT(index.has_value(), "a parameter's ID is one of the instrument's");
    return *index;
}

} // namespace irama_studio
