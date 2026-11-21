// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <irama/engine/base/narrow.hpp>

namespace irama_studio {

/// Combines ImGui flags, which are plain `int`s and enumerators, as ImGui's `|` does, without
/// a bitwise operation on a signed value.
template <typename... Flags> [[nodiscard]] int combine_flags(Flags... flags) {
    return irama::narrow<int>((irama::narrow<unsigned>(static_cast<int>(flags)) | ...));
}

} // namespace irama_studio
