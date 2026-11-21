// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

/// \file
/// The ID that names a parameter.

#pragma once

#include <cstdint>

namespace irama {

/// The ID of a parameter, unique within its owner kind: an instrument, or one device type.
///
/// Every ID is written out in its table, never derived from a position, so reordering a table
/// changes no ID. A strong type, so that an ID can't be taken for an index or a value.
enum class ParamId : std::uint32_t {};

} // namespace irama
