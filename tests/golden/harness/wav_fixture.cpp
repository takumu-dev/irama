// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

// Writes a WAV file for the golden harness's checks: a sine wave at half of full scale, the same on
// every channel, at any rate, channel count and length, so that a golden can be wrong in any one of
// them.
//
//   irama_wav_fixture <file.wav> <rate> <channels> <frames> <hertz>

#include <irama/engine/base/narrow.hpp>
#include <irama/engine/wav/wav_writer.hpp>

#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <numbers>
#include <optional>
#include <print>
#include <span>
#include <string_view>
#include <system_error>
#include <vector>

#ifdef _MSC_VER
#include <cstdlib>
#endif

namespace {

template <typename Number> [[nodiscard]] std::optional<Number> parse(std::string_view text) {
    Number value{};
    const char* const last = std::to_address(text.end());
    const auto [end, error] = std::from_chars(std::to_address(text.begin()), last, value);
    if (error != std::errc{} || end != last) {
        return std::nullopt;
    }
    return value;
}

} // namespace

// NOLINTNEXTLINE(bugprone-exception-escape): nothing is caught, so a failure ends the program.
int main(int argc, char** argv) {
#ifdef _MSC_VER
    // MSVC's runtime answers abort() with a dialog in debug builds, and with a Windows Error
    // Reporting fault report, either of which can wait for a click that never comes in CI.
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    const std::span arguments{argv, irama::narrow<std::size_t>(argc)};
    if (arguments.size() != 6) {
        std::println(stderr, "usage: irama_wav_fixture <file.wav> <rate> <channels> <frames> "
                             "<hertz>");
        return 2;
    }
    // The program's own name comes first, and each argument is read in turn.
    auto words = arguments.subspan(1);
    const auto next = [&words] {
        char* const word = words.front();
        words = words.subspan(1);
        return word;
    };
    const char* const path = next();
    const auto rate = parse<std::uint32_t>(next());
    const auto channels = parse<std::uint16_t>(next());
    const auto frames = parse<std::size_t>(next());
    const auto frequency = parse<double>(next());
    if (!rate || !channels || !frames || !frequency) {
        std::println(stderr, "irama_wav_fixture: the rate, channels, frames and hertz are numbers");
        return 2;
    }

    std::vector<float> samples;
    samples.reserve(*frames * *channels);
    for (std::size_t frame = 0; frame < *frames; ++frame) {
        const double time = static_cast<double>(frame) / *rate;
        const auto sample =
            static_cast<float>(0.5 * std::sin(2.0 * std::numbers::pi * *frequency * time));
        samples.insert(samples.end(), *channels, sample);
    }
    auto writer = irama::WavWriter::create(path, {.sample_rate = *rate, .channels = *channels});
    if (!writer || !writer->write(samples) || !writer->finish()) {
        std::println(stderr, "irama_wav_fixture: couldn't write {}", path);
        return 1;
    }
    return 0;
}
