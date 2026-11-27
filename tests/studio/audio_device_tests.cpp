// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

// The audio wrapper on miniaudio's Null backend, a device driven by a timer on miniaudio's own
// thread, which needs no sound hardware.

#include "audio/audio_device.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <thread>
#include <vector>

using irama_studio::AudioBackend;
using irama_studio::AudioDevice;
using irama_studio::AudioGranted;
using irama_studio::AudioRequest;

namespace {

constexpr std::array null_backend{AudioBackend::null};

// What the callback saw, from the audio thread.
struct Calls {
    std::atomic<std::uint32_t> count = 0;
    // The length of a span that wasn't the stereo samples of the frames asked for, or 0.
    std::atomic<std::size_t> wrong_length = 0;
};

[[nodiscard]] AudioDevice::Callback counting(Calls& calls, std::size_t frames) {
    return [&calls, frames](std::span<float> out) {
        if (out.size() != frames * 2) {
            calls.wrong_length.store(out.size(), std::memory_order_relaxed);
        }
        calls.count.fetch_add(1, std::memory_order_relaxed);
    };
}

// Waits for the callback to have run `count` times, or for a deadline far longer than that takes
// even under a sanitizer. Returns whether it ran that often.
[[nodiscard]] bool wait_for_calls(const Calls& calls, std::uint32_t count) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{10};
    while (calls.count.load(std::memory_order_relaxed) < count) {
        if (std::chrono::steady_clock::now() > deadline) {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }
    return true;
}

} // namespace

TEST_CASE("the Null backend opens, reports what it granted and calls back", "[studio][audio]") {
    AudioDevice device{null_backend};
    REQUIRE(device.backend() == AudioBackend::null);
    CHECK_FALSE(device.is_open());

    Calls calls;
    REQUIRE(device.open(AudioRequest{.sample_rate = 44'100, .callback_frames = 128},
                        counting(calls, 128)));
    CHECK(device.is_open());
    REQUIRE(device.granted());
    const AudioGranted granted = device.granted().value_or(AudioGranted{});
    CHECK(granted.backend == AudioBackend::null);
    CHECK(granted.sample_rate == 44'100);
    CHECK(granted.callback_frames == 128);
    CHECK_FALSE(granted.device_name.empty());
    CHECK(device.last_error().empty());

    // Opening doesn't start the callback, so the caller can prepare for the rate it was granted.
    std::this_thread::sleep_for(std::chrono::milliseconds{20});
    CHECK(calls.count == 0);

    REQUIRE(device.start());
    CHECK(wait_for_calls(calls, 8));
    CHECK(calls.wrong_length == 0);

    device.close();
    CHECK_FALSE(device.is_open());
    CHECK_FALSE(device.granted());
    const std::uint32_t after_close = calls.count;
    std::this_thread::sleep_for(std::chrono::milliseconds{20});
    CHECK(calls.count == after_close);
}

TEST_CASE("a device opened at rate 0 runs at the device's own rate", "[studio][audio]") {
    AudioDevice device{null_backend};
    Calls calls;
    REQUIRE(
        device.open(AudioRequest{.sample_rate = 0, .callback_frames = 256}, counting(calls, 256)));
    REQUIRE(device.granted());
    const AudioGranted granted = device.granted().value_or(AudioGranted{});
    CHECK(granted.sample_rate > 0);
    CHECK(granted.callback_frames == 256);
}

TEST_CASE("a device opens again after it is closed", "[studio][audio]") {
    AudioDevice device{null_backend};
    Calls first;
    REQUIRE(device.open(AudioRequest{.callback_frames = 256}, counting(first, 256)));
    REQUIRE(device.start());
    REQUIRE(wait_for_calls(first, 2));

    // Opening an open device closes it first.
    Calls second;
    REQUIRE(device.open(AudioRequest{.callback_frames = 512}, counting(second, 512)));
    const std::uint32_t first_count = first.count;
    REQUIRE(device.start());
    CHECK(wait_for_calls(second, 2));
    CHECK(second.wrong_length == 0);
    CHECK(first.count == first_count);
}

TEST_CASE("with no backend the device doesn't open and says why", "[studio][audio]") {
    AudioDevice device{std::span<const AudioBackend>{}};
    CHECK_FALSE(device.backend());
    Calls calls;
    CHECK_FALSE(device.open(AudioRequest{}, counting(calls, 256)));
    CHECK_FALSE(device.is_open());
    CHECK_FALSE(device.last_error().empty());
    CHECK_FALSE(device.start());
    device.close();
}

TEST_CASE("the Null backend lists its device by name, and opens it by that name",
          "[studio][audio]") {
    AudioDevice device{null_backend};
    const std::vector<std::string> names = device.device_names();
    REQUIRE(names.size() == 1);
    CHECK_FALSE(names.front().empty());

    Calls calls;
    REQUIRE(device.open(AudioRequest{.device_name = names.front(), .callback_frames = 256},
                        counting(calls, 256)));
    REQUIRE(device.granted());
    CHECK(device.granted().value_or(AudioGranted{}).device_name == names.front());
    REQUIRE(device.start());
    CHECK(wait_for_calls(calls, 2));
}

TEST_CASE("a device name that isn't listed doesn't open, and says why", "[studio][audio]") {
    AudioDevice device{null_backend};
    Calls calls;
    CHECK_FALSE(device.open(AudioRequest{.device_name = "No Such Device", .callback_frames = 256},
                            counting(calls, 256)));
    CHECK_FALSE(device.is_open());
    CHECK(device.last_error().contains("No Such Device"));
}

TEST_CASE("with no backend there are no devices to list", "[studio][audio]") {
    const AudioDevice device{std::span<const AudioBackend>{}};
    CHECK(device.device_names().empty());
}
