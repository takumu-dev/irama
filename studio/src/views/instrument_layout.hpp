// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <irama/engine/param/param_descriptor.hpp>

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace irama_studio {

/// The widget that changes a parameter in the Instrument window.
enum class ParamWidget : std::uint8_t {
    /// A slider that moves in normalised values, for a linear or logarithmic parameter.
    slider,
    /// A combo box of its labels, for a stepped parameter with labels.
    combo,
    /// A slider over whole numbers, for a stepped parameter without labels.
    integer_slider,
};

/// \return The widget that changes the parameter, chosen by its scale and labels.
[[nodiscard]] ParamWidget widget_for(const irama::ParamDescriptor& param);

/// The parameters of one part of the instrument, under one collapsing header.
struct ParamGroup {
    /// The header's title.
    std::string_view title;
    /// The parameters, in the order of their table.
    std::vector<const irama::ParamDescriptor*> params;

    friend bool operator==(const ParamGroup&, const ParamGroup&) = default;
};

/// Sorts a table's parameters into groups by their ID's block: the oscillator for the source's
/// blocks, from 10000, then the filter (the 1000s), the amplifier (the 2000s), the envelope
/// (the 3000s) and the voice (the 4000s). A parameter in no known block goes in a last group,
/// Other, so that nothing in the table goes unshown.
///
/// \param table The parameters, which stay where they are while the groups point into them.
/// \return The groups in that order, leaving out any with no parameters.
[[nodiscard]] std::vector<ParamGroup> group_params(std::span<const irama::ParamDescriptor> table);

} // namespace irama_studio
