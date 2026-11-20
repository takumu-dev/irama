// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include <irama/engine/base/assertion.hpp>

#include "realtime_sanitizer.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>

namespace irama::detail {

void fail_assertion(const char* expression, const char* message, std::source_location location) {
#ifdef IRAMA_REALTIME_SANITIZER
    // The program is ending, so the report may break the real-time rules, which RealtimeSanitizer
    // would otherwise report in its place.
    const __rtsan::ScopedDisabler real_time_checks_off;
#endif
    // A check may fail on the audio thread, so the report keeps to <cstdio>.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,modernize-use-std-print): it may allocate.
    std::fprintf(stderr, "%s:%lu: assertion `%s` failed: %s\n", location.file_name(),
                 static_cast<unsigned long>(location.line()), expression, message);
    // abort() flushes no stream, and a host may have given stderr a buffer.
    std::fflush(stderr);
    std::abort();
}

} // namespace irama::detail
