// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/fonts.hpp"

#include <irama/engine/base/narrow.hpp>

#include <imgui.h>

#include <span>

namespace irama_studio {

void load_fonts() {
    const std::span<const unsigned char> ttf = inter_regular_ttf();
    // The bytes live as long as the program, so the atlas reads them in place and never frees them.
    ImFontConfig config;
    config.FontDataOwnedByAtlas = false;
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast): ImGui only reads data it doesn't own
    void* const data = const_cast<unsigned char*>(ttf.data());
    ImGui::GetIO().Fonts->AddFontFromMemoryTTF(data, irama::narrow<int>(ttf.size()), ui_font_size,
                                               &config);
}

} // namespace irama_studio
