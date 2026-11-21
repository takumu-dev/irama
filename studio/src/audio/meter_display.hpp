// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <chrono>
#include <concepts>
#include <optional>

namespace irama_studio {

/// What the status bar shows of the audio callback's load and clipping, taken on the UI thread
/// from what the callback reports.
class MeterDisplay {
  public:
    using Clock = std::chrono::steady_clock;

    /// How often the CPU peak is taken. Each value shown is the peak since the one before.
    static constexpr Clock::duration cpu_interval = std::chrono::milliseconds{500};
    /// How long CLIP shows after the last clipped sample.
    static constexpr Clock::duration clip_hold = std::chrono::seconds{1};

    /// Brings the display up to `now`, once a frame.
    ///
    /// \param now The time of the frame.
    /// \param take_cpu_peak Returns the callback's CPU peak since it was last taken, and resets it.
    ///                      It is called only when the interval has passed.
    /// \param clipped Whether the callback has clipped a sample since the last update.
    template <std::invocable Take>
    void update(Clock::time_point now, Take take_cpu_peak, bool clipped) {
        if (!cpu_taken_ || now - *cpu_taken_ >= cpu_interval) {
            cpu_peak_ = take_cpu_peak();
            cpu_taken_ = now;
        }
        if (clipped) {
            last_clip_ = now;
        }
        clipping_ = last_clip_ && now - *last_clip_ < clip_hold;
    }

    /// \return The CPU peak to show: the callback's longest run as a share of its period.
    [[nodiscard]] float cpu_peak() const { return cpu_peak_; }

    /// \return Whether to show CLIP.
    [[nodiscard]] bool clipping() const { return clipping_; }

  private:
    float cpu_peak_ = 0.0F;
    std::optional<Clock::time_point> cpu_taken_;
    std::optional<Clock::time_point> last_clip_;
    bool clipping_ = false;
};

} // namespace irama_studio
