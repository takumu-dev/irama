// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "app/options.hpp"

namespace irama_studio {

/// The exit status when Studio can't start, such as when there is no display or no GPU device.
inline constexpr int exit_failure = 1;
/// The exit status for a command line Studio doesn't understand, as getopt-style tools use.
inline constexpr int exit_usage = 2;

/// Opens Studio's window and runs it until the user quits, or until it has drawn the frames that
/// the options ask for, and returns the program's exit status. If SDL's video or the GPU device
/// fails, it shows SDL's error in a message box, prints it to stderr, and returns `exit_failure`.
[[nodiscard]] int run(const Options& options);

} // namespace irama_studio
