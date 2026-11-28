// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <irama/engine/param/param_descriptor.hpp>
#include <irama/engine/param/param_id.hpp>

#include <cstddef>
#include <span>
#include <vector>

namespace irama_studio {

/// Studio's own copy of the instrument's parameter values, by ID, which the Instrument window shows
/// and changes, and every new player is given as instrument 01.
///
/// It starts from each parameter's default.
class InstrumentValues {
  public:
    /// Makes the copy with every parameter at its default.
    ///
    /// \param table The parameters, which outlive the copy.
    explicit InstrumentValues(std::span<const irama::ParamDescriptor> table);

    /// \return The parameters the copy holds values for.
    [[nodiscard]] std::span<const irama::ParamDescriptor> table() const { return table_; }

    /// \param id The ID of one of the table's parameters, which is checked.
    /// \return The parameter's value in the copy.
    [[nodiscard]] float value(irama::ParamId id) const;

    /// \return Every value, in the order of the table.
    [[nodiscard]] std::span<const float> values() const { return values_; }

    /// Sets a parameter's value, constrained to the parameter: clamped to its range, and rounded to
    /// the nearest whole number if it is stepped.
    ///
    /// \param id The ID of one of the table's parameters, which is checked.
    /// \param value The new plain value, any number but NaN.
    void set(irama::ParamId id, float value);

  private:
    // The index of a parameter in the table, which is checked.
    [[nodiscard]] std::size_t index_of(irama::ParamId id) const;

    std::span<const irama::ParamDescriptor> table_;
    std::vector<float> values_;
};

} // namespace irama_studio
