// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <chrono>
#include <cstdint>
#include <optional>

namespace irama_studio {

/// Watches the audio callback's heartbeat from the UI thread, to find a device that has stopped
/// calling back. No backend reports a lost device, so a heartbeat that stands still is the sign.
class HeartbeatWatch {
  public:
    using Clock = std::chrono::steady_clock;

    /// How long the heartbeat stands still before the device counts as lost.
    static constexpr Clock::duration timeout = std::chrono::seconds{1};

    /// Looks at the heartbeat, once a frame while a device is open.
    ///
    /// \param now The time of the frame.
    /// \param heartbeat How many times the callback has run.
    /// \return Whether the heartbeat has stood still for `timeout`.
    [[nodiscard]] bool lost(Clock::time_point now, std::uint64_t heartbeat) {
        if (!moved_ || heartbeat != heartbeat_) {
            heartbeat_ = heartbeat;
            moved_ = now;
        }
        return now - *moved_ >= timeout;
    }

    /// Forgets what it has seen, as when a device opens, so the next look starts the timeout.
    void reset() { moved_.reset(); }

  private:
    std::uint64_t heartbeat_ = 0;
    // When the heartbeat was last seen to move.
    std::optional<Clock::time_point> moved_;
};

} // namespace irama_studio
