// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

// The main of every Catch2 test program, in place of Catch2's own.

#include <catch2/catch_session.hpp>

#ifdef _MSC_VER
#include <cstdlib>
#endif

int main(int argc, char* argv[]) {
#ifdef _MSC_VER
    // MSVC's runtime answers abort() with a dialog in debug builds, and with a Windows Error
    // Reporting fault report, either of which can wait for a click that never comes in CI. Without
    // them, a test that aborts ends at once and fails.
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    return Catch::Session().run(argc, argv);
}
