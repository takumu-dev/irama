// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include <irama/engine/base/assertion.hpp>

#include <cstdio>
#include <cstdlib>
#include <source_location>

// RealtimeSanitizer's header comes with the sanitizer, so it is included only in a build that has
// it on. Compilers without __has_feature have no RealtimeSanitizer.
#ifdef __has_feature
#if __has_feature(realtime_sanitizer)
#define IRAMA_REALTIME_SANITIZER
#include <sanitizer/rtsan_interface.h>
#endif
#endif

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
