// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cstddef>
#include <cstdio>
#include <print>
#include <span>
#include <string_view>

namespace irama_cli {
namespace {

// The exit status of a command line irama-cli doesn't understand, as getopt-style tools use.
constexpr int usage_error = 2;

int run(std::span<char*> arguments) {
    if (arguments.size() == 2 && std::string_view{arguments.back()} == "--version") {
        std::println("irama-cli {}", IRAMA_VERSION_STRING);
        return 0;
    }
    std::println(stderr, "usage: irama-cli --version");
    return usage_error;
}

} // namespace
} // namespace irama_cli

// NOLINTNEXTLINE(bugprone-exception-escape): nothing is caught, so a failure ends the program.
int main(int argc, char** argv) { return irama_cli::run({argv, static_cast<std::size_t>(argc)}); }
