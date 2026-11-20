// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

// Compares the WAV file a golden case wrote with its golden, sample by sample:
//
//   irama_wav_compare <actual> <golden> --max-error <error>
//
// Both must be 32-bit float, with the same rate, channel count and length, and no sample may differ
// from its golden by more than the error. It prints the peak of the actual file and the largest
// difference, and exits 0 if they match, 1 if they don't, and 2 for a command line it doesn't
// understand. It reads the files with dr_wav, which was written apart from Irama's WAV writer, so
// it checks the writer's headers too.

#include <irama/engine/base/narrow.hpp>

#define DR_WAV_IMPLEMENTATION
#include <dr_wav.h>

#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <memory>
#include <optional>
#include <print>
#include <ranges>
#include <span>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#ifdef _MSC_VER
#include <cstdlib>
#endif

namespace {

constexpr int mismatch = 1;
constexpr int usage_error = 2;

struct Wav {
    std::uint16_t format = 0;
    std::uint16_t bits_per_sample = 0;
    std::uint16_t channels = 0;
    std::uint32_t sample_rate = 0;
    std::uint64_t frames = 0;
    std::vector<float> samples;
};

[[nodiscard]] std::optional<Wav> read_wav(const char* path) {
    drwav file;
    if (drwav_init_file(&file, path, nullptr) == DRWAV_FALSE) {
        return std::nullopt;
    }
    Wav wav{
        .format = file.translatedFormatTag,
        .bits_per_sample = file.bitsPerSample,
        .channels = file.channels,
        .sample_rate = file.sampleRate,
        .frames = file.totalPCMFrameCount,
        .samples = {},
    };
    wav.samples.resize(irama::narrow<std::size_t>(wav.frames) * wav.channels);
    const drwav_uint64 read = drwav_read_pcm_frames_f32(&file, wav.frames, wav.samples.data());
    drwav_uninit(&file);
    if (read != wav.frames) {
        return std::nullopt;
    }
    return wav;
}

// Where a sample sits, and its value or its difference from the golden.
struct Extreme {
    double value = 0.0;
    std::size_t index = 0;
};

// Checks what must match exactly, printing each difference.
bool same_layout(const Wav& actual, const char* actual_path, const Wav& golden,
                 const char* golden_path) {
    bool same = true;
    const auto check_float = [&](const Wav& wav, const char* path) {
        if (wav.format != DR_WAVE_FORMAT_IEEE_FLOAT || wav.bits_per_sample != 32) {
            std::println(stderr, "{} isn't 32-bit float: its format is {}, with {} bits a sample",
                         path, wav.format, wav.bits_per_sample);
            same = false;
        }
    };
    check_float(actual, actual_path);
    check_float(golden, golden_path);
    const auto differs = [&](std::string_view what, auto actual_value, auto golden_value) {
        if (actual_value != golden_value) {
            std::println(stderr, "{} has {} {}, but {} has {}", actual_path, actual_value, what,
                         golden_path, golden_value);
            same = false;
        }
    };
    differs("frames a second", actual.sample_rate, golden.sample_rate);
    differs("channels", actual.channels, golden.channels);
    differs("frames", actual.frames, golden.frames);
    return same;
}

int compare(const char* actual_path, const char* golden_path, double max_error) {
    const auto read = [](const char* path) {
        auto wav = read_wav(path);
        if (!wav) {
            std::println(stderr, "{} isn't a WAV file that dr_wav can read", path);
        }
        return wav;
    };
    const auto actual = read(actual_path);
    const auto golden = read(golden_path);
    if (!actual || !golden || !same_layout(*actual, actual_path, *golden, golden_path)) {
        return mismatch;
    }

    Extreme peak;
    Extreme largest_error;
    std::size_t over = 0;
    for (std::size_t index = 0; const auto [actual_sample, golden_sample] :
                                std::views::zip(actual->samples, golden->samples)) {
        const auto sample = double{actual_sample};
        const double difference = std::abs(sample - double{golden_sample});
        // A NaN sample compares false with everything, so it is counted as an infinite error.
        const double error =
            std::isnan(difference) ? std::numeric_limits<double>::infinity() : difference;
        if (std::abs(sample) > peak.value) {
            peak = {.value = std::abs(sample), .index = index};
        }
        if (error > largest_error.value) {
            largest_error = {.value = error, .index = index};
        }
        if (error > max_error) {
            ++over;
        }
        ++index;
    }
    const auto where = [&](const Extreme& extreme) {
        return std::pair{extreme.index / actual->channels, extreme.index % actual->channels};
    };
    const auto [peak_frame, peak_channel] = where(peak);
    const auto [error_frame, error_channel] = where(largest_error);
    std::println("peak: {} at frame {}, channel {}", peak.value, peak_frame, peak_channel);
    std::println("largest error: {} at frame {}, channel {}, with {} of {} samples over {}",
                 largest_error.value, error_frame, error_channel, over, actual->samples.size(),
                 max_error);
    if (over > 0) {
        std::println(stderr, "{} differs from {}", actual_path, golden_path);
        return mismatch;
    }
    return 0;
}

[[nodiscard]] std::optional<double> parse_error(std::string_view text) {
    double value = 0.0;
    const char* const last = std::to_address(text.end());
    const auto [end, error] = std::from_chars(std::to_address(text.begin()), last, value);
    if (error != std::errc{} || end != last || std::isnan(value) || value < 0.0) {
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
    // The program's own name comes first.
    const auto words = arguments.empty() ? arguments : arguments.subspan(1);
    std::optional<double> max_error;
    if (words.size() == 4 && std::string_view{words.last(2).front()} == "--max-error") {
        max_error = parse_error(words.back());
    }
    if (!max_error) {
        std::println(stderr, "usage: irama_wav_compare <actual> <golden> --max-error <error>");
        return usage_error;
    }
    return compare(words.front(), words.subspan(1).front(), *max_error);
}
