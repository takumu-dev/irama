// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include <irama/engine/voice/instrument.hpp>

#include <irama/engine/base/assertion.hpp>
#include <irama/engine/base/nonblocking.hpp>
#include <irama/engine/param/param_descriptor.hpp>
#include <irama/engine/param/param_id.hpp>
#include <irama/engine/param/param_smoother.hpp>
#include <irama/engine/source/oscillator.hpp>
#include <irama/engine/source/tuning.hpp>
#include <irama/engine/voice/adsr.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <ranges>
#include <span>
#include <tuple>
#include <utility>

namespace {

using irama::InstrumentParam;

// The index of a parameter in the instrument's table, found when compiling.
[[nodiscard]] consteval std::size_t index_of(irama::ParamId id) {
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access): consteval, so a missing ID can't compile.
    return irama::find_param(irama::instrument_params, id).value();
}

constexpr std::size_t osc1_waveform = index_of(InstrumentParam::osc1_waveform);
constexpr std::size_t osc1_level = index_of(InstrumentParam::osc1_level);
constexpr std::size_t osc1_pulse_width = index_of(InstrumentParam::osc1_pulse_width);
constexpr std::size_t osc1_coarse = index_of(InstrumentParam::osc1_coarse);
constexpr std::size_t osc1_fine = index_of(InstrumentParam::osc1_fine);
constexpr std::size_t noise_level = index_of(InstrumentParam::noise_level);
constexpr std::size_t gain = index_of(InstrumentParam::gain);
constexpr std::size_t pan = index_of(InstrumentParam::pan);
constexpr std::size_t velocity_sensitivity = index_of(InstrumentParam::velocity_sensitivity);
constexpr std::size_t attack = index_of(InstrumentParam::attack);
constexpr std::size_t decay = index_of(InstrumentParam::decay);
constexpr std::size_t sustain = index_of(InstrumentParam::sustain);
constexpr std::size_t release = index_of(InstrumentParam::release);
constexpr std::size_t attack_curve = index_of(InstrumentParam::attack_curve);
constexpr std::size_t decay_curve = index_of(InstrumentParam::decay_curve);
constexpr std::size_t release_curve = index_of(InstrumentParam::release_curve);

constexpr float percent = 100.0F;
constexpr float cents_per_semitone = 100.0F;

// Renders a percentage parameter's values as fractions from 0 to 1.
void render_fraction(const irama::ParamSmoother& param, std::span<float> values) IRAMA_NONBLOCKING {
    param.render(values);
    for (float& value : values) {
        value /= percent;
    }
}

template <std::size_t... Index>
[[nodiscard]] std::array<irama::ParamSmoother, sizeof...(Index)>
make_smoothers(float sample_rate, std::index_sequence<Index...> /*indices*/) {
    return {irama::ParamSmoother{std::get<Index>(irama::instrument_params), sample_rate}...};
}

// The amplifier's gain on each channel, for its gain in decibels and its pan. Both sides take the
// sine of their angle, so that a centred pan, whose angles are both π/4 exactly in float, is the
// same on each side to the bit.
[[nodiscard]] std::pair<float, float> amplifier_gains(float decibels, float pan_position) {
    constexpr float quarter_turn = std::numbers::pi_v<float> / 2.0F;
    const float linear = std::pow(10.0F, decibels / 20.0F);
    const float theta = (pan_position + 1.0F) * (quarter_turn / 2.0F);
    return {linear * std::sin(quarter_turn - theta), linear * std::sin(theta)};
}

} // namespace

namespace irama {

Instrument::Instrument(float sample_rate)
    : sample_rate_{sample_rate},
      params_{make_smoothers(sample_rate, std::make_index_sequence<instrument_params.size()>{})} {
    IRAMA_ASSERT(sample_rate > 0.0F, "a sample rate is above 0 Hz");
}

void Instrument::set(std::size_t index, float value, bool smooth) IRAMA_NONBLOCKING {
    IRAMA_ASSERT(index < params_.size(), "a parameter's index is within the instrument's table");
    params_.at(index).set(value, smooth);
}

float Instrument::value(std::size_t index) const {
    IRAMA_ASSERT(index < params_.size(), "a parameter's index is within the instrument's table");
    return params_.at(index).value();
}

float Instrument::sample_rate() const { return sample_rate_; }

Waveform Instrument::waveform() const {
    // A stepped value holds a whole number within its range.
    return static_cast<Waveform>(static_cast<int>(params_.at(osc1_waveform).value()));
}

AdsrSettings Instrument::envelope() const {
    return AdsrSettings{
        .attack_seconds = params_.at(attack).value(),
        .decay_seconds = params_.at(decay).value(),
        .release_seconds = params_.at(release).value(),
        .attack_curve = params_.at(attack_curve).value(),
        .decay_curve = params_.at(decay_curve).value(),
        .release_curve = params_.at(release_curve).value(),
    };
}

float Instrument::velocity_gain(float velocity) const {
    const float sensitivity = params_.at(velocity_sensitivity).value() / percent;
    return 1.0F - (sensitivity * (1.0F - (velocity * velocity)));
}

InstrumentBlock Instrument::render(std::size_t frames) IRAMA_NONBLOCKING {
    IRAMA_ASSERT(frames <= max_block_frames, "an instrument computes at most one block at once");
    const auto osc_levels = std::span{osc_level_}.first(frames);
    const auto pulse_widths = std::span{pulse_width_}.first(frames);
    const auto tunings = std::span{tuning_}.first(frames);
    const auto noise_levels = std::span{noise_level_}.first(frames);
    const auto sustain_levels = std::span{sustain_}.first(frames);
    const auto left = std::span{left_gain_}.first(frames);
    const auto right = std::span{right_gain_}.first(frames);

    render_fraction(params_.at(osc1_level), osc_levels);
    render_fraction(params_.at(osc1_pulse_width), pulse_widths);
    render_fraction(params_.at(noise_level), noise_levels);
    render_fraction(params_.at(sustain), sustain_levels);
    // The coarse tuning is stepped, so it holds for the block.
    const float coarse = params_.at(osc1_coarse).value();
    if (params_.at(osc1_fine).ramping()) {
        // The fine tuning's cents go through the tunings' buffer on the way.
        params_.at(osc1_fine).render(tunings);
        for (float& tuning : tunings) {
            tuning = tuning_ratio(coarse + (tuning / cents_per_semitone));
        }
    } else {
        std::ranges::fill(
            tunings, tuning_ratio(coarse + (params_.at(osc1_fine).value() / cents_per_semitone)));
    }
    if (params_.at(gain).ramping() || params_.at(pan).ramping()) {
        // The gain's decibels and the pan's positions go through the gains' buffers on the way.
        params_.at(gain).render(left);
        params_.at(pan).render(right);
        for (const auto [left_gain, right_gain] : std::views::zip(left, right)) {
            std::tie(left_gain, right_gain) = amplifier_gains(left_gain, right_gain);
        }
    } else {
        const auto [left_gain, right_gain] =
            amplifier_gains(params_.at(gain).value(), params_.at(pan).value());
        std::ranges::fill(left, left_gain);
        std::ranges::fill(right, right_gain);
    }

    const Waveform block_waveform = waveform();
    const AdsrSettings settings = envelope();
    for (ParamSmoother& param : params_) {
        param.advance(frames);
    }
    return InstrumentBlock{
        .waveform = block_waveform,
        .osc_level = osc_levels,
        .pulse_width = pulse_widths,
        .tuning = tunings,
        .noise_level = noise_levels,
        .envelope = settings,
        .sustain = sustain_levels,
        .left_gain = left,
        .right_gain = right,
    };
}

} // namespace irama
