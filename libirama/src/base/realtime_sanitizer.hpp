// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

// RealtimeSanitizer's interface, in a build that has it on, which then defines
// IRAMA_REALTIME_SANITIZER. Its header comes with the sanitizer, so no other build can include it.
// Compilers without __has_feature have no RealtimeSanitizer.

#pragma once

// A file includes this for the macro, which most builds never define, so include checks would take
// the include for an unused one.
// IWYU pragma: always_keep

#ifdef __has_feature
#if __has_feature(realtime_sanitizer)
#define IRAMA_REALTIME_SANITIZER
#include <sanitizer/rtsan_interface.h>
#endif
#endif
