// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include <irama/engine/voice/instrument.hpp>

#include <irama/engine/base/nonblocking.hpp>
#include <irama/engine/base/render_scope.hpp>
#include <irama/engine/filter/svf.hpp>
#include <irama/engine/param/param_descriptor.hpp>
#include <irama/engine/param/param_id.hpp>
#include <irama/engine/source/oscillator.hpp>
#include <irama/engine/source/tuning.hpp>
#include <irama/engine/voice/adsr.hpp>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <span>
#include <string_view>
#include <vector>

namespace {

using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;
using irama::InstrumentParam;
using irama::ParamScale;

constexpr float rate = 48'000.0F;

// 20 ms at 48 kHz.
constexpr std::size_t ramp_frames = 960;

// The index of one of the instrument's parameters, or one past the table's end, which any use
// of it catches, for an ID it hasn't.
[[nodiscard]] std::size_t index_of(irama::ParamId id) {
    return irama::find_param(irama::instrument_params, id)
        .value_or(irama::instrument_params.size());
}

// One instrument's values for the voices, gathered over several blocks.
struct Rendered {
    std::vector<float> sustain;
    std::vector<float> left_gain;
    std::vector<float> right_gain;
    std::vector<float> gain;
    std::vector<float> osc_level;
    std::vector<float> pulse_width;
    std::vector<float> tuning;
    std::vector<float> noise_level;
    std::vector<irama::SvfCoefficients> filter;
    // The waveform, the filter's mode and the filter's slope of each block.
    std::vector<irama::Waveform> waveform;
    std::vector<irama::FilterMode> filter_mode;
    std::vector<irama::FilterSlope> filter_slope;
};

// Appends one block's values to those gathered so far.
template <typename Value> void append(std::vector<Value>& gathered, std::span<const Value> block) {
    gathered.insert(gathered.end(), block.begin(), block.end());
}

// Renders as the player does, inside a render scope, so the real-time checks watch the instrument.
[[nodiscard]] irama::InstrumentBlock render_block(irama::Instrument& instrument,
                                                  irama::InstrumentBuffers& buffers,
                                                  std::size_t frames) IRAMA_NONBLOCKING {
    const irama::RenderScope scope;
    return instrument.render(buffers, frames);
}

// Renders the instrument's next frames in the largest blocks it takes.
[[nodiscard]] Rendered render(irama::Instrument& instrument, std::size_t frames) {
    Rendered rendered;
    irama::InstrumentBuffers buffers;
    for (std::size_t done = 0; done < frames; done += irama::Instrument::max_block_frames) {
        const irama::InstrumentBlock block = render_block(
            instrument, buffers, std::min(irama::Instrument::max_block_frames, frames - done));
        append(rendered.sustain, block.sustain);
        append(rendered.left_gain, block.left_gain);
        append(rendered.right_gain, block.right_gain);
        append(rendered.gain, block.gain);
        append(rendered.osc_level, block.osc_level);
        append(rendered.pulse_width, block.pulse_width);
        append(rendered.tuning, block.tuning);
        append(rendered.noise_level, block.noise_level);
        append(rendered.filter, block.filter);
        rendered.waveform.push_back(block.waveform);
        rendered.filter_mode.push_back(block.filter_mode);
        rendered.filter_slope.push_back(block.filter_slope);
    }
    return rendered;
}

[[nodiscard]] double decibels(double gain) { return 20.0 * std::log10(gain); }

// One row of the basic synth's table, as the instrument must declare it.
struct Row {
    std::uint32_t id = 0;
    float min = 0.0F;
    float max = 0.0F;
    float default_value = 0.0F;
    ParamScale scale = ParamScale::linear;
    float smoothing_seconds = 0.0F;
};

constexpr auto basic_synth = std::to_array<Row>({
    {.id = 10'000, .max = 3, .default_value = 2, .scale = ParamScale::stepped},
    {.id = 10'001, .max = 100, .default_value = 100, .smoothing_seconds = 0.02F},
    {.id = 10'002, .min = 5, .max = 95, .default_value = 50, .smoothing_seconds = 0.02F},
    {.id = 10'003, .min = -24, .max = 24, .scale = ParamScale::stepped},
    {.id = 10'004, .min = -100, .max = 100, .smoothing_seconds = 0.02F},
    {.id = 10'090, .max = 100, .smoothing_seconds = 0.02F},
    {.id = 1'000, .max = 3, .scale = ParamScale::stepped},
    {.id = 1'001, .max = 1, .scale = ParamScale::stepped},
    {
        .id = 1'002,
        .min = 20,
        .max = 20'000,
        .default_value = 20'000,
        .scale = ParamScale::logarithmic,
        .smoothing_seconds = 0.02F,
    },
    {.id = 1'003, .max = 100, .smoothing_seconds = 0.02F},
    {.id = 2'000, .min = -60, .max = 12, .default_value = -12, .smoothing_seconds = 0.02F},
    {.id = 2'001, .min = -1, .max = 1, .smoothing_seconds = 0.02F},
    {.id = 2'002, .max = 100, .default_value = 100},
    {
        .id = 3'000,
        .min = 0.001F,
        .max = 10,
        .default_value = 0.002F,
        .scale = ParamScale::logarithmic,
    },
    {
        .id = 3'001,
        .min = 0.001F,
        .max = 10,
        .default_value = 0.3F,
        .scale = ParamScale::logarithmic,
    },
    {.id = 3'002, .max = 100, .default_value = 70, .smoothing_seconds = 0.02F},
    {
        .id = 3'003,
        .min = 0.001F,
        .max = 20,
        .default_value = 0.2F,
        .scale = ParamScale::logarithmic,
    },
    {.id = 3'004, .min = -1, .max = 1},
    {.id = 3'005, .min = -1, .max = 1, .default_value = 0.5F},
    {.id = 3'006, .min = -1, .max = 1, .default_value = 0.5F},
    {.id = 4'000, .max = 2, .scale = ParamScale::stepped},
});

[[nodiscard]] bool matches(const irama::ParamDescriptor& param, const Row& row) {
    return param.min == row.min && param.max == row.max &&
           param.default_value == row.default_value && param.scale == row.scale &&
           param.smoothing_seconds == row.smoothing_seconds;
}

} // namespace

TEST_CASE("the instrument's table is well formed", "[voice]") {
    REQUIRE(irama::find_table_fault(irama::instrument_params) == irama::TableFault::none);
    for (const irama::ParamDescriptor& param : irama::instrument_params) {
        REQUIRE(!param.name.empty());
        REQUIRE(param.automatable);
        // A stepped parameter can be automated but not modulated, and a continuous one can be both.
        REQUIRE(param.modulatable == (param.scale != ParamScale::stepped));
    }
}

TEST_CASE("the instrument declares every parameter of the basic synth", "[voice]") {
    REQUIRE(irama::instrument_params.size() == basic_synth.size());
    for (const Row& row : basic_synth) {
        CAPTURE(row.id);
        REQUIRE(matches(irama::instrument_params.at(index_of(irama::ParamId{row.id})), row));
    }
    const auto& waveform = irama::instrument_params.at(index_of(InstrumentParam::osc1_waveform));
    REQUIRE(waveform.labels.size() == 4);
    REQUIRE(waveform.labels.subspan(2, 1).front() == "Saw");
    const auto& action = irama::instrument_params.at(index_of(InstrumentParam::new_note_action));
    REQUIRE(std::ranges::equal(action.labels,
                               std::array<std::string_view, 3>{"Note-off", "Cut", "Continue"}));
}

TEST_CASE("the new note action follows its stepped value", "[voice]") {
    irama::Instrument instrument{rate};
    REQUIRE(instrument.new_note_action() == irama::NewNoteAction::note_off);
    instrument.set(index_of(InstrumentParam::new_note_action), 1.0F, false);
    REQUIRE(instrument.new_note_action() == irama::NewNoteAction::cut);
    instrument.set(index_of(InstrumentParam::new_note_action), 2.0F, false);
    REQUIRE(instrument.new_note_action() == irama::NewNoteAction::carry_on);
}

TEST_CASE("the amplifier's gain before the pan is its decibels as a linear gain", "[voice]") {
    irama::Instrument instrument{rate};
    instrument.set(index_of(InstrumentParam::pan), 0.5F, false);
    const auto rendered = render(instrument, 300);
    const float linear = std::pow(10.0F, -12.0F / 20.0F);
    const auto [left_pan, right_pan] = irama::pan_gains(0.5F);
    for (std::size_t frame = 0; frame < 300; ++frame) {
        REQUIRE(rendered.gain.at(frame) == linear);
        REQUIRE(rendered.left_gain.at(frame) == linear * left_pan);
        REQUIRE(rendered.right_gain.at(frame) == linear * right_pan);
    }
}

TEST_CASE("while the amplifier glides, its left and right gains are its gain times the pan's",
          "[voice]") {
    irama::Instrument instrument{rate};
    instrument.set(index_of(InstrumentParam::pan), 0.5F, false);
    static_cast<void>(render(instrument, 300));
    const float linear = std::pow(10.0F, -12.0F / 20.0F);
    instrument.set(index_of(InstrumentParam::gain), 0.0F, true);
    instrument.set(index_of(InstrumentParam::pan), -0.5F, true);
    const auto rendered = render(instrument, 1'000);
    for (std::size_t frame = 0; frame < 1'000; ++frame) {
        REQUIRE(rendered.gain.at(frame) > linear);
        REQUIRE(rendered.gain.at(frame) <= 1.0F);
    }
    REQUIRE(rendered.gain.back() == 1.0F);
    const auto [last_left, last_right] = irama::pan_gains(-0.5F);
    REQUIRE(rendered.left_gain.back() == last_left);
    REQUIRE(rendered.right_gain.back() == last_right);
}

TEST_CASE("an instrument starts with every parameter at its default", "[voice]") {
    const irama::Instrument instrument{rate};
    for (std::size_t index = 0; index < irama::instrument_params.size(); ++index) {
        REQUIRE(instrument.value(index) == irama::instrument_params.at(index).default_value);
    }
    const irama::AdsrSettings envelope = instrument.envelope();
    REQUIRE(envelope.attack_seconds == 0.002F);
    REQUIRE(envelope.decay_seconds == 0.3F);
    REQUIRE(envelope.release_seconds == 0.2F);
    REQUIRE(envelope.attack_curve == 0.0F);
    REQUIRE(envelope.decay_curve == 0.5F);
    REQUIRE(envelope.release_curve == 0.5F);
}

TEST_CASE("a centred pan is 3 dB down on each side", "[voice]") {
    irama::Instrument instrument{rate};
    instrument.set(index_of(InstrumentParam::gain), 0.0F, false);
    const auto rendered = render(instrument, 10);
    for (std::size_t frame = 0; frame < 10; ++frame) {
        REQUIRE_THAT(decibels(static_cast<double>(rendered.left_gain.at(frame))),
                     WithinAbs(-3.0103, 1e-4));
        REQUIRE(rendered.right_gain.at(frame) == rendered.left_gain.at(frame));
    }
}

TEST_CASE("the pan keeps constant power from hard left to hard right", "[voice]") {
    irama::Instrument instrument{rate};
    const std::size_t pan = index_of(InstrumentParam::pan);
    const double gain = std::pow(10.0, -12.0 / 20.0);

    instrument.set(pan, -1.0F, false);
    auto rendered = render(instrument, 1);
    REQUIRE_THAT(rendered.left_gain.front(), WithinRel(gain, 1e-6));
    REQUIRE(rendered.right_gain.front() == 0.0F);

    instrument.set(pan, 1.0F, false);
    rendered = render(instrument, 1);
    REQUIRE(rendered.left_gain.front() == 0.0F);
    REQUIRE_THAT(rendered.right_gain.front(), WithinRel(gain, 1e-6));

    instrument.set(pan, 0.5F, false);
    rendered = render(instrument, 1);
    const double theta = 0.75 * std::numbers::pi / 2.0;
    REQUIRE_THAT(rendered.left_gain.front(), WithinRel(gain * std::cos(theta), 1e-5));
    REQUIRE_THAT(rendered.right_gain.front(), WithinRel(gain * std::sin(theta), 1e-5));
}

TEST_CASE("a smoothed gain ramps linearly in decibels over exactly 20 ms", "[voice]") {
    irama::Instrument instrument{rate};
    instrument.set(index_of(InstrumentParam::gain), 0.0F, true);
    const auto rendered = render(instrument, ramp_frames + 50);
    const double centre = std::cos(std::numbers::pi / 4.0);
    for (std::size_t frame = 0; frame < ramp_frames; ++frame) {
        const double db = -12.0 + (12.0 * static_cast<double>(frame + 1) / ramp_frames);
        REQUIRE_THAT(rendered.left_gain.at(frame),
                     WithinRel(std::pow(10.0, db / 20.0) * centre, 1e-5));
    }
    REQUIRE(rendered.left_gain.at(ramp_frames - 2) < rendered.left_gain.at(ramp_frames - 1));
    for (std::size_t frame = ramp_frames; frame < rendered.left_gain.size(); ++frame) {
        REQUIRE(rendered.left_gain.at(frame) == rendered.left_gain.at(ramp_frames - 1));
    }
}

TEST_CASE("an instrument's value set without smoothing jumps at once", "[voice]") {
    irama::Instrument instrument{rate};
    const std::size_t sustain = index_of(InstrumentParam::sustain);
    instrument.set(sustain, 25.0F, false);
    REQUIRE(instrument.value(sustain) == 25.0F);
    const auto rendered = render(instrument, 300);
    REQUIRE(rendered.sustain == std::vector<float>(300, 0.25F));
}

TEST_CASE("an instrument moved on without its values computed glides as one computed", "[voice]") {
    const std::size_t gain = index_of(InstrumentParam::gain);
    irama::Instrument rendered{rate};
    irama::Instrument advanced{rate};
    rendered.set(gain, 0.0F, true);
    advanced.set(gain, 0.0F, true);
    REQUIRE(advanced.target(gain) == 0.0F);
    REQUIRE(advanced.value(gain) == -12.0F);
    static_cast<void>(render(rendered, 300));
    advanced.advance(300);
    REQUIRE(advanced.value(gain) == rendered.value(gain));
    REQUIRE(render(advanced, ramp_frames).left_gain == render(rendered, ramp_frames).left_gain);
}

TEST_CASE("the sustain level follows its smoothed value, as a level from 0 to 1", "[voice]") {
    irama::Instrument instrument{rate};
    instrument.set(index_of(InstrumentParam::sustain), 20.0F, true);
    const auto rendered = render(instrument, ramp_frames);
    REQUIRE_THAT(rendered.sustain.front(), WithinAbs(0.7 - (0.5 / ramp_frames), 1e-6));
    REQUIRE_THAT(rendered.sustain.at((ramp_frames / 2) - 1), WithinAbs(0.45, 1e-6));
    REQUIRE(rendered.sustain.back() == 0.2F);
}

TEST_CASE("a note's velocity gain is 1 - s(1 - v^2)", "[voice]") {
    irama::Instrument instrument{rate};
    const std::size_t sensitivity = index_of(InstrumentParam::velocity_sensitivity);
    REQUIRE(instrument.velocity_gain(1.0F) == 1.0F);
    REQUIRE(instrument.velocity_gain(0.5F) == 0.25F);
    REQUIRE(instrument.velocity_gain(0.0F) == 0.0F);

    instrument.set(sensitivity, 0.0F, false);
    REQUIRE(instrument.velocity_gain(0.0F) == 1.0F);
    REQUIRE(instrument.velocity_gain(0.3F) == 1.0F);

    instrument.set(sensitivity, 50.0F, false);
    REQUIRE_THAT(instrument.velocity_gain(0.5F), WithinAbs(0.625, 1e-7));
    REQUIRE_THAT(instrument.velocity_gain(0.0F), WithinAbs(0.5, 1e-7));
}

TEST_CASE("the envelope's settings are the parameters' values as they are now", "[voice]") {
    irama::Instrument instrument{rate};
    instrument.set(index_of(InstrumentParam::attack), 0.5F, true);
    instrument.set(index_of(InstrumentParam::decay), 1.5F, true);
    instrument.set(index_of(InstrumentParam::release), 12.0F, true);
    instrument.set(index_of(InstrumentParam::attack_curve), -1.0F, true);
    instrument.set(index_of(InstrumentParam::decay_curve), 0.25F, true);
    instrument.set(index_of(InstrumentParam::release_curve), 1.0F, true);
    const irama::AdsrSettings envelope = instrument.envelope();
    REQUIRE(envelope.attack_seconds == 0.5F);
    REQUIRE(envelope.decay_seconds == 1.5F);
    REQUIRE(envelope.release_seconds == 12.0F);
    REQUIRE(envelope.attack_curve == -1.0F);
    REQUIRE(envelope.decay_curve == 0.25F);
    REQUIRE(envelope.release_curve == 1.0F);
    REQUIRE(render(instrument, 1).sustain.front() == 0.7F);
}

TEST_CASE("the instrument's source values start at their defaults", "[voice]") {
    irama::Instrument instrument{rate};
    REQUIRE(instrument.waveform() == irama::Waveform::saw);
    const Rendered rendered = render(instrument, 300);
    REQUIRE(rendered.waveform == std::vector{irama::Waveform::saw, irama::Waveform::saw});
    REQUIRE(rendered.osc_level == std::vector<float>(300, 1.0F));
    REQUIRE(rendered.pulse_width == std::vector<float>(300, 0.5F));
    REQUIRE(rendered.tuning == std::vector<float>(300, 1.0F));
    REQUIRE(rendered.noise_level == std::vector<float>(300, 0.0F));
}

TEST_CASE("the levels and the pulse width are fractions of their percentages", "[voice]") {
    irama::Instrument instrument{rate};
    instrument.set(index_of(InstrumentParam::osc1_level), 25.0F, false);
    instrument.set(index_of(InstrumentParam::osc1_pulse_width), 10.0F, false);
    instrument.set(index_of(InstrumentParam::noise_level), 80.0F, false);
    const Rendered rendered = render(instrument, 10);
    REQUIRE(rendered.osc_level == std::vector<float>(10, 0.25F));
    REQUIRE(rendered.pulse_width == std::vector<float>(10, 0.1F));
    REQUIRE(rendered.noise_level == std::vector<float>(10, 0.8F));
}

TEST_CASE("the waveform follows its stepped value", "[voice]") {
    irama::Instrument instrument{rate};
    for (const auto waveform :
         {irama::Waveform::sine, irama::Waveform::triangle, irama::Waveform::pulse}) {
        instrument.set(index_of(InstrumentParam::osc1_waveform),
                       static_cast<float>(static_cast<int>(waveform)), false);
        REQUIRE(instrument.waveform() == waveform);
        REQUIRE(render(instrument, 1).waveform.front() == waveform);
    }
}

TEST_CASE("the tuning is the ratio of the coarse semitones and the fine cents", "[voice]") {
    irama::Instrument instrument{rate};
    instrument.set(index_of(InstrumentParam::osc1_coarse), 7.0F, false);
    instrument.set(index_of(InstrumentParam::osc1_fine), -50.0F, false);
    const Rendered rendered = render(instrument, 10);
    for (const float ratio : rendered.tuning) {
        REQUIRE(ratio == irama::tuning_ratio(6.5F));
    }
}

TEST_CASE("a coarse step changes the tuning at once, and a fine change glides over 20 ms",
          "[voice]") {
    irama::Instrument instrument{rate};
    instrument.set(index_of(InstrumentParam::osc1_coarse), 12.0F, true);
    instrument.set(index_of(InstrumentParam::osc1_fine), 100.0F, true);
    const Rendered rendered = render(instrument, ramp_frames + 10);
    // The coarse step applies from the first frame, the fine glide arrives on the ramp's last.
    REQUIRE_THAT(rendered.tuning.front(),
                 WithinRel(irama::tuning_ratio(12.0F + (1.0F / 960.0F)), 1e-6F));
    for (std::size_t frame = 1; frame < ramp_frames; ++frame) {
        REQUIRE(rendered.tuning.at(frame) > rendered.tuning.at(frame - 1));
    }
    for (std::size_t frame = ramp_frames - 1; frame < rendered.tuning.size(); ++frame) {
        REQUIRE(rendered.tuning.at(frame) == irama::tuning_ratio(13.0F));
    }
}

TEST_CASE("the filter starts low-pass at 12 dB, open at 20 kHz with no resonance", "[voice]") {
    irama::Instrument instrument{rate};
    REQUIRE(instrument.filter_mode() == irama::FilterMode::low_pass);
    REQUIRE(instrument.filter_slope() == irama::FilterSlope::db12);
    const Rendered rendered = render(instrument, 300);
    REQUIRE(rendered.filter_mode ==
            std::vector{irama::FilterMode::low_pass, irama::FilterMode::low_pass});
    REQUIRE(rendered.filter_slope ==
            std::vector{irama::FilterSlope::db12, irama::FilterSlope::db12});
    REQUIRE(rendered.filter == std::vector(300, irama::svf_coefficients(20'000.0F, 0.0F, rate)));
}

TEST_CASE("the filter's mode and slope follow their stepped values", "[voice]") {
    irama::Instrument instrument{rate};
    instrument.set(index_of(InstrumentParam::filter_mode), 3.0F, false);
    instrument.set(index_of(InstrumentParam::filter_slope), 1.0F, false);
    REQUIRE(instrument.filter_mode() == irama::FilterMode::notch);
    REQUIRE(instrument.filter_slope() == irama::FilterSlope::db24);
    const Rendered rendered = render(instrument, 1);
    REQUIRE(rendered.filter_mode.front() == irama::FilterMode::notch);
    REQUIRE(rendered.filter_slope.front() == irama::FilterSlope::db24);
}

TEST_CASE("the filter's coefficients are those of the cutoff and the resonance as a fraction",
          "[voice]") {
    irama::Instrument instrument{rate};
    instrument.set(index_of(InstrumentParam::cutoff), 1'000.0F, false);
    instrument.set(index_of(InstrumentParam::resonance), 50.0F, false);
    REQUIRE(render(instrument, 10).filter ==
            std::vector(10, irama::svf_coefficients(1'000.0F, 0.5F, rate)));
}

TEST_CASE("the filter's coefficients follow the smoothed cutoff and resonance every frame",
          "[voice]") {
    irama::Instrument instrument{rate};
    instrument.set(index_of(InstrumentParam::cutoff), 200.0F, true);
    instrument.set(index_of(InstrumentParam::resonance), 100.0F, true);
    const Rendered rendered = render(instrument, ramp_frames + 10);
    // The cutoff falls and the resonance rises on every frame of the ramp, and arrive on its last.
    for (std::size_t frame = 1; frame < ramp_frames; ++frame) {
        REQUIRE(rendered.filter.at(frame).g < rendered.filter.at(frame - 1).g);
        REQUIRE(rendered.filter.at(frame).resonant.damping <
                rendered.filter.at(frame - 1).resonant.damping);
    }
    const irama::SvfCoefficients arrived = irama::svf_coefficients(200.0F, 1.0F, rate);
    for (std::size_t frame = ramp_frames - 1; frame < rendered.filter.size(); ++frame) {
        REQUIRE(rendered.filter.at(frame) == arrived);
    }
}
