// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <irama/engine/param/param_descriptor.hpp>

#include <optional>
#include <string>
#include <string_view>

namespace irama_studio {

/// Writes a parameter's plain value as text: a stepped parameter's label where it has labels, and
/// otherwise the number to the parameter's precision and its unit's symbol, such as `440 Hz`,
/// `-12.0 dB` or `50.0%`. Semitones and cents above 0 take a `+`, and a value that rounds to 0 is
/// never written `-0`.
///
/// \param param The parameter.
/// \param value A plain value of the parameter.
/// \return The text.
[[nodiscard]] std::string format_param_value(const irama::ParamDescriptor& param, float value);

/// Reads a value the user typed for a parameter: a number, with or without the unit's symbol, or
/// for a parameter with labels, one of its labels in any case.
///
/// \param param The parameter.
/// \param text What the user typed, with any spaces around it.
/// \return The value, constrained to the parameter, or nothing if the text isn't one.
[[nodiscard]] std::optional<float> parse_param_value(const irama::ParamDescriptor& param,
                                                     std::string_view text);

} // namespace irama_studio
