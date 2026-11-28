// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

// The main of every Catch2 test program, in place of Catch2's own.

#include <catch2/catch_session.hpp>

#ifdef _MSC_VER
#include <crtdbg.h>

#include <cstdlib>
#endif

int main(int argc, char* argv[]) {
#ifdef _MSC_VER
    // MSVC's runtime answers abort() with a dialog in debug builds, and with a Windows Error
    // Reporting fault report, either of which can wait for a click that never comes in CI. Without
    // them, a test that aborts ends at once and fails.
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
    return Catch::Session().run(argc, argv);
}
