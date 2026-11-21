// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

// miniaudio's implementation, which its header holds. It compiles here, with the flags of
// Studio's own sources, so each preset's sanitizer instruments miniaudio's threads too.

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>
