// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "app/application.hpp"
#include "app/options.hpp"
#include <irama/engine/base/narrow.hpp>

#include <cstddef>
#include <cstdio>
#include <print>
#include <span>
#include <string_view>
#include <vector>

// NOLINTNEXTLINE(bugprone-exception-escape): nothing is caught, so a failure ends the program.
int main(int argc, char** argv) {
    const std::span<char*> all{argv, irama::narrow<std::size_t>(argc)};
    const std::span<char*> after_name = all.empty() ? all : all.subspan(1);
    const std::vector<std::string_view> arguments(after_name.begin(), after_name.end());
    const auto options = irama_studio::parse_options(arguments);
    if (!options) {
        std::println(stderr, "{}", options.error());
        return irama_studio::exit_usage;
    }
    return irama_studio::run(*options);
}
