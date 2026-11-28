// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "script.hpp"
#include <irama/engine/base/allocator.hpp>
#include <irama/engine/base/narrow.hpp>
#include <irama/engine/base/nonblocking.hpp>
#include <irama/engine/base/render_scope.hpp>
#include <irama/engine/model/change_set.hpp>
#include <irama/engine/model/instrument_part.hpp>
#include <irama/engine/model/song_part.hpp>
#include <irama/engine/param/param_descriptor.hpp>
#include <irama/engine/param/param_id.hpp>
#include <irama/engine/player/player.hpp>
#include <irama/engine/voice/instrument.hpp>
#include <irama/engine/wav/wav_writer.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <ios>
#include <iterator>
#include <memory>
#include <new>
#include <numbers>
#include <optional>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#ifdef _MSC_VER
#include <crtdbg.h>

#include <cstdlib>
#endif

namespace irama_cli {
namespace {

// The exit status of a command that failed, such as a file that couldn't be written.
constexpr int failure = 1;
// The exit status of a command line irama-cli doesn't understand, as getopt-style tools use.
constexpr int usage_error = 2;

constexpr std::string_view usage =
    "usage: irama-cli --version\n"
    "       irama-cli tone <file.wav> [--freq <hertz>] [--seconds <seconds>]\n"
    "       irama-cli synth <file.wav> --script <file> [--seconds <seconds>] [--rate <hertz>]";

// The tone is written as the golden tests compare it: 48 kHz, stereo.
constexpr std::uint32_t tone_rate = 48'000;
constexpr std::uint16_t tone_channels = 2;
// Half of full scale, which is comfortable to listen to and leaves headroom on either side.
constexpr double tone_amplitude = 0.5;
// An hour, far beyond any test, keeps the file well inside what a WAV file can hold.
constexpr double max_seconds = 3'600.0;

// The sample rates a synth can be rendered at, and the one it is rendered at unless --rate says
// otherwise.
constexpr std::array<std::uint32_t, 4> synth_rates{44'100, 48'000, 88'200, 96'000};
constexpr std::uint32_t default_synth_rate = 48'000;
constexpr std::uint16_t synth_channels = 2;

struct ToneOptions {
    const char* path = nullptr;
    double frequency = 440.0;
    double seconds = 1.0;
};

// Reads a whole argument as a finite number.
[[nodiscard]] std::optional<double> parse_number(std::string_view text) {
    double value = 0.0;
    const char* const last = std::to_address(text.end());
    const auto [end, error] = std::from_chars(std::to_address(text.begin()), last, value);
    if (error != std::errc{} || end != last || !std::isfinite(value)) {
        return std::nullopt;
    }
    return value;
}

// Reads the arguments after `tone`: one path and the options, in any order. Prints what is wrong
// with them, if anything.
[[nodiscard]] std::optional<ToneOptions> parse_tone(std::span<char*> arguments) {
    ToneOptions options;
    while (!arguments.empty()) {
        const char* const argument = arguments.front();
        arguments = arguments.subspan(1);
        const std::string_view name = argument;
        if (!name.starts_with("--")) {
            if (options.path != nullptr) {
                std::println(stderr, "{}", usage);
                return std::nullopt;
            }
            options.path = argument;
            continue;
        }
        std::optional<double> value;
        if (!arguments.empty()) {
            value = parse_number(arguments.front());
            arguments = arguments.subspan(1);
        }
        if (name == "--freq") {
            if (!value || *value <= 0.0 || *value >= tone_rate / 2.0) {
                std::println(stderr,
                             "irama-cli: --freq takes a number of hertz above 0 and below {}",
                             tone_rate / 2);
                return std::nullopt;
            }
            options.frequency = *value;
        } else if (name == "--seconds") {
            if (!value || *value <= 0.0 || *value > max_seconds) {
                std::println(stderr,
                             "irama-cli: --seconds takes a number of seconds above 0, up to {}",
                             max_seconds);
                return std::nullopt;
            }
            options.seconds = *value;
        } else {
            std::println(stderr, "{}", usage);
            return std::nullopt;
        }
    }
    if (options.path == nullptr) {
        std::println(stderr, "{}", usage);
        return std::nullopt;
    }
    return options;
}

// Renders the tone's frames from `first` on into `block`, the same sample on both channels, as a
// render call would, so the real-time checks watch it.
void render_tone(std::span<float> block, double frequency, std::size_t first) IRAMA_NONBLOCKING {
    const irama::RenderScope scope;
    for (std::size_t offset = 0; offset < block.size() / tone_channels; ++offset) {
        // The cycles so far, less the whole ones, so that sin's argument stays within one turn
        // however long the tone runs.
        const double cycles =
            std::fmod(frequency * static_cast<double>(first + offset), tone_rate) / tone_rate;
        const auto sample =
            static_cast<float>(tone_amplitude * std::sin(2.0 * std::numbers::pi * cycles));
        std::ranges::fill(block.subspan(offset * tone_channels, tone_channels), sample);
    }
}

// Writes a sine wave at the frequency, the same on both channels, for the seconds given, rounded
// to the nearest frame.
int tone(const ToneOptions& options) {
    auto writer = irama::WavWriter::create(options.path,
                                           {.sample_rate = tone_rate, .channels = tone_channels});
    if (!writer) {
        std::println(stderr, "irama-cli: couldn't open {} for writing", options.path);
        return failure;
    }
    const auto frames = irama::narrow<std::size_t>(std::llround(options.seconds * tone_rate));
    constexpr std::size_t block_frames = 512;
    std::array<float, block_frames * tone_channels> block{};
    bool written = true;
    for (std::size_t start = 0; written && start < frames; start += block_frames) {
        const auto samples =
            std::span{block}.first(std::min(block_frames, frames - start) * tone_channels);
        render_tone(samples, options.frequency, start);
        written = writer->write(samples).has_value();
    }
    if (!written || !writer->finish()) {
        std::println(stderr, "irama-cli: couldn't write {}", options.path);
        return failure;
    }
    return 0;
}

// Reads a whole argument as a whole number of hertz.
[[nodiscard]] std::optional<std::uint32_t> parse_rate(std::string_view text) {
    std::uint32_t value = 0;
    const char* const last = std::to_address(text.end());
    const auto [end, error] = std::from_chars(std::to_address(text.begin()), last, value);
    if (error != std::errc{} || end != last) {
        return std::nullopt;
    }
    return value;
}

struct SynthOptions {
    const char* path = nullptr;
    const char* script = nullptr;
    double seconds = 1.0;
    std::uint32_t rate = default_synth_rate;
};

// Applies the option `name`, given `value` or null if the command line ends after it. Prints what
// is wrong with it, if anything.
[[nodiscard]] bool apply_synth_option(SynthOptions& options, std::string_view name,
                                      const char* value) {
    if (name == "--script" && value != nullptr) {
        options.script = value;
        return true;
    }
    if (name == "--seconds") {
        const auto seconds = value != nullptr ? parse_number(value) : std::nullopt;
        if (!seconds || *seconds <= 0.0 || *seconds > max_seconds) {
            std::println(stderr, "irama-cli: --seconds takes a number of seconds above 0, up to {}",
                         max_seconds);
            return false;
        }
        options.seconds = *seconds;
        return true;
    }
    if (name == "--rate") {
        const auto rate = value != nullptr ? parse_rate(value) : std::nullopt;
        if (!rate || !std::ranges::contains(synth_rates, *rate)) {
            std::println(stderr, "irama-cli: --rate takes 44100, 48000, 88200 or 96000");
            return false;
        }
        options.rate = *rate;
        return true;
    }
    std::println(stderr, "{}", usage);
    return false;
}

// Reads the arguments after `synth`: one path and the options, in any order. Prints what is wrong
// with them, if anything.
[[nodiscard]] std::optional<SynthOptions> parse_synth(std::span<char*> arguments) {
    SynthOptions options;
    while (!arguments.empty()) {
        const char* const argument = arguments.front();
        arguments = arguments.subspan(1);
        const std::string_view name = argument;
        if (!name.starts_with("--")) {
            if (options.path != nullptr) {
                std::println(stderr, "{}", usage);
                return std::nullopt;
            }
            options.path = argument;
            continue;
        }
        const char* value = nullptr;
        if (!arguments.empty()) {
            value = arguments.front();
            arguments = arguments.subspan(1);
        }
        if (!apply_synth_option(options, name, value)) {
            return std::nullopt;
        }
    }
    if (options.path == nullptr || options.script == nullptr) {
        std::println(stderr, "{}", usage);
        return std::nullopt;
    }
    return options;
}

// A script plays its notes on this instrument, the only one its song has.
constexpr int script_instrument = 0x01;

// The one instrument a script plays, whose values its set-parameter lines change. Each change
// reaches the player as a change set holding the whole instrument.
class ScriptInstrument {
  public:
    ScriptInstrument() {
        std::ranges::transform(irama::instrument_params, values_.begin(),
                               &irama::ParamDescriptor::default_value);
    }

    // Sets one of the instrument's parameters, constrained to it, as the player would have.
    void set(irama::ParamId id, float value) {
        const std::optional<std::size_t> index = irama::find_param(irama::instrument_params, id);
        if (index) {
            values_.at(*index) = irama::instrument_params.at(*index).constrain(value);
        }
    }

    // A change set of the instrument as it is now, and of the new song's song part if `with_song`.
    [[nodiscard]] irama::ChangeSetPtr change(bool with_song) const {
        const irama::Allocator allocator = irama::default_allocator();
        auto changes = irama::ChangeSet::create(allocator, {.instruments = 1});
        auto part = irama::InstrumentPart::create(allocator, values_);
        auto song = irama::SongPart::create(allocator);
        if (!changes || !part || !song) {
            throw std::bad_alloc{};
        }
        (*changes)->replace_instrument(script_instrument, std::move(*part));
        if (with_song) {
            (*changes)->replace_song(std::move(*song));
        }
        return std::move(*changes);
    }

  private:
    std::array<float, irama::instrument_params.size()> values_{};
};

// Queues one checked message of a script.
[[nodiscard]] bool send(irama::Player& player, ScriptInstrument& instrument,
                        const ScriptMessage& message) {
    switch (message.kind) {
    case ScriptMessage::Kind::note_on:
        return player.note_on(message.frame, message.note, message.velocity, script_instrument);
    case ScriptMessage::Kind::note_off:
        return player.note_off(message.frame, message.note, script_instrument);
    case ScriptMessage::Kind::release_all:
        return player.release_all(message.frame);
    case ScriptMessage::Kind::set_parameter: {
        // Setting the same value again changes nothing, so a refused line can be sent again.
        instrument.set(message.param, message.value);
        irama::ChangeSetPtr changes = instrument.change(false);
        return player.change(message.frame, changes);
    }
    }
    return false;
}

// Plays a script through a player, for the seconds given, rounded to the nearest frame, and writes
// what it renders. The whole script is read and checked before the player sees any of it.
int synth(const SynthOptions& options) {
    std::ifstream file{options.script, std::ios::binary};
    if (!file.is_open()) {
        std::println(stderr, "irama-cli: couldn't read {}", options.script);
        return failure;
    }
    const std::string text{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
    const auto messages = parse_script(text);
    if (!messages) {
        std::println(stderr, "irama-cli: {}:{}: {}", options.script, messages.error().line,
                     messages.error().reason);
        return usage_error;
    }

    auto player = irama::Player::create({.sample_rate = options.rate});
    if (!player) {
        std::println(stderr, "irama-cli: out of memory");
        return failure;
    }
    // The song: the new song's song part, and instrument 01 at every default, before anything
    // renders.
    ScriptInstrument instrument;
    irama::ChangeSetPtr song = instrument.change(true);
    if (!(*player)->change(0, song)) {
        std::println(stderr, "irama-cli: the player refused the song");
        return failure;
    }
    auto writer = irama::WavWriter::create(
        options.path, {.sample_rate = options.rate, .channels = synth_channels});
    if (!writer) {
        std::println(stderr, "irama-cli: couldn't open {} for writing", options.path);
        return failure;
    }

    const auto frames = irama::narrow<std::uint64_t>(std::llround(options.seconds * options.rate));
    constexpr std::uint64_t block_frames = 4'096;
    std::vector<float> block(block_frames * synth_channels);
    auto next = messages->begin();
    bool written = true;
    for (std::uint64_t start = 0; written && start < frames;) {
        std::uint64_t end = std::min(start + block_frames, frames);
        // Each message is queued before the render that reaches its frame. A full queue holds the
        // rest until the frames before the next of them are rendered, which empties it. Only more
        // messages at one frame than the queue holds would make the last of them a frame late.
        for (; next != messages->end() && next->frame < end; ++next) {
            if (!send(**player, instrument, *next)) {
                end = std::max(next->frame, start + 1);
                break;
            }
        }
        const auto samples =
            std::span{block}.first(irama::narrow<std::size_t>(end - start) * synth_channels);
        (*player)->render(samples);
        // Dropping the change sets the player hands back frees the parts they replaced.
        while ((*player)->take_returned() != nullptr) {
        }
        written = writer->write(samples).has_value();
        start = end;
    }
    if (!written || !writer->finish()) {
        std::println(stderr, "irama-cli: couldn't write {}", options.path);
        return failure;
    }
    return 0;
}

int run(std::span<char*> arguments) {
    // The program's own name comes first.
    const auto words = arguments.empty() ? arguments : arguments.subspan(1);
    if (words.size() == 1 && std::string_view{words.front()} == "--version") {
        std::println("irama-cli {}", IRAMA_VERSION_STRING);
        return 0;
    }
    if (!words.empty() && std::string_view{words.front()} == "tone") {
        const auto options = parse_tone(words.subspan(1));
        return options ? tone(*options) : usage_error;
    }
    if (!words.empty() && std::string_view{words.front()} == "synth") {
        const auto options = parse_synth(words.subspan(1));
        return options ? synth(*options) : usage_error;
    }
    std::println(stderr, "{}", usage);
    return usage_error;
}

} // namespace
} // namespace irama_cli

// NOLINTNEXTLINE(bugprone-exception-escape): nothing is caught, so a failure ends the program.
int main(int argc, char** argv) {
#ifdef _MSC_VER
    // MSVC's runtime answers abort() with a dialog in debug builds, and with a Windows Error
    // Reporting fault report, either of which can wait for a click that never comes in a script or
    // in CI. Without them, abort() ends the program at once.
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#ifdef _DEBUG
    // The debug runtime's assertion and error reports, from _ASSERTE, the standard library's
    // checks, /RTC and the debug heap, go to stderr rather than to a dialog.
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
#endif
#endif
    return irama_cli::run({argv, irama::narrow<std::size_t>(argc)});
}
