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

#ifdef _MSC_VER
#include <crtdbg.h>

#include <cstdlib>
#endif

// NOLINTNEXTLINE(bugprone-exception-escape): nothing is caught, so a failure ends the program.
int main(int argc, char** argv) {
#ifdef _MSC_VER
    // MSVC's runtime answers abort() with a dialog in debug builds, and with a Windows Error
    // Reporting fault report, either of which can wait for a click that never comes in a test.
    // Without them, abort() ends the program at once.
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#ifdef _DEBUG
    // The debug runtime's assertion and error reports, from _ASSERTE, the standard library's
    // checks, /RTC and the debug heap, go to stderr rather than to a dialog.
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
#endif
#endif
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
