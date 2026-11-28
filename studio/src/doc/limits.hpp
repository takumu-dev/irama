// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "doc/changed_parts.hpp"
#include "doc/document.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace irama_studio {

/// Checks the limits of the Document that a change to `changed` could break: everything when the
/// song part changed, which reshapes every pattern, and otherwise the patterns and instruments
/// named.
///
/// \return The first limit broken, worded for the status bar, or nothing.
[[nodiscard]] std::optional<std::string> broken_limit(const Document& document,
                                                      const ChangedParts& changed);

/// Whether a section or song parameter name is one that game code can look up: not empty, and with
/// no space at either end.
[[nodiscard]] bool is_lookup_name(std::string_view name);

} // namespace irama_studio
