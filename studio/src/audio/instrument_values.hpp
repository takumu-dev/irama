// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <irama/engine/param/param_descriptor.hpp>
#include <irama/engine/param/param_id.hpp>

#include <concepts>
#include <cstddef>
#include <ranges>
#include <span>
#include <vector>

namespace irama_studio {

/// Studio's own copy of the instrument's parameter values, by ID, which the Instrument window shows
/// and changes, and every new player is given.
///
/// It starts from each parameter's default. Each value changed and not yet taken by a player waits
/// to be sent, so a value the player's queue refuses goes again on the next flush, and only its
/// latest value goes.
class InstrumentValues {
  public:
    /// Makes the copy with every parameter at its default, and nothing waiting to be sent.
    ///
    /// \param table The parameters, which outlive the copy.
    explicit InstrumentValues(std::span<const irama::ParamDescriptor> table);

    /// \return The parameters the copy holds values for.
    [[nodiscard]] std::span<const irama::ParamDescriptor> table() const { return table_; }

    /// \param id The ID of one of the table's parameters, which is checked.
    /// \return The parameter's value in the copy.
    [[nodiscard]] float value(irama::ParamId id) const;

    /// Sets a parameter's value, constrained to the parameter as a player would, and marks it to
    /// be sent.
    ///
    /// \param id The ID of one of the table's parameters, which is checked.
    /// \param value The new plain value, any number but NaN.
    void set(irama::ParamId id, float value);

    /// Marks every value to be sent, as for a new player.
    void send_all();

    /// Sends the values waiting through `deliver`, in the order of the table, until one is refused.
    ///
    /// \param deliver Sends one parameter's value, and returns whether it was taken.
    template <std::predicate<irama::ParamId, float> Deliver> void flush(Deliver deliver) {
        for (auto&& [param, entry] : std::views::zip(table_, entries_)) {
            if (entry.waiting) {
                if (!deliver(param.id, entry.value)) {
                    return;
                }
                entry.waiting = false;
            }
        }
    }

    /// \return Whether any value waits to be sent.
    [[nodiscard]] bool waiting() const;

  private:
    struct Entry {
        float value = 0.0F;
        bool waiting = false;
    };

    // The index of a parameter in the table, which is checked.
    [[nodiscard]] std::size_t index_of(irama::ParamId id) const;

    std::span<const irama::ParamDescriptor> table_;
    std::vector<Entry> entries_;
};

} // namespace irama_studio
