// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

/// \file
/// The instrument: its parameters, their live values, and what every voice reads from them.

#pragma once

#include <irama/engine/base/nonblocking.hpp>
#include <irama/engine/filter/svf.hpp>
#include <irama/engine/param/param_descriptor.hpp>
#include <irama/engine/param/param_id.hpp>
#include <irama/engine/param/param_smoother.hpp>
#include <irama/engine/source/oscillator.hpp>
#include <irama/engine/voice/adsr.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <utility>

namespace irama {

/// The IDs of the instrument's parameters.
///
/// IDs fall in decimal blocks, one for each part of the instrument: the filter at 1000, the
/// amplifier at 2000, the amplifier's envelope at 3000, the voice at 4000, and the subtractive
/// source at 10000, whose oscillator `n` starts at `10000 + 10 · (n − 1)` and whose noise is at
/// 10090.
struct InstrumentParam {
    /// The oscillator's waveform: sine, triangle, saw or pulse.
    static constexpr ParamId osc1_waveform{10'000};
    /// The oscillator's level, in percent.
    static constexpr ParamId osc1_level{10'001};
    /// The pulse's width, in percent.
    static constexpr ParamId osc1_pulse_width{10'002};
    /// The oscillator's coarse tuning, in semitones.
    static constexpr ParamId osc1_coarse{10'003};
    /// The oscillator's fine tuning, in cents.
    static constexpr ParamId osc1_fine{10'004};
    /// The noise's level, in percent.
    static constexpr ParamId noise_level{10'090};
    /// The filter's mode: low-pass, high-pass, band-pass or notch.
    static constexpr ParamId filter_mode{1'000};
    /// The filter's slope: 12 or 24 dB per octave.
    static constexpr ParamId filter_slope{1'001};
    /// The filter's cutoff, in hertz.
    static constexpr ParamId cutoff{1'002};
    /// The filter's resonance, in percent.
    static constexpr ParamId resonance{1'003};
    /// The amplifier's gain, in decibels.
    static constexpr ParamId gain{2'000};
    /// The pan, from −1, hard left, to +1, hard right.
    static constexpr ParamId pan{2'001};
    /// How much the velocity sets a note's level, in percent.
    static constexpr ParamId velocity_sensitivity{2'002};
    /// The amplifier envelope's attack, in seconds.
    static constexpr ParamId attack{3'000};
    /// The amplifier envelope's decay, in seconds.
    static constexpr ParamId decay{3'001};
    /// The amplifier envelope's sustain level, in percent.
    static constexpr ParamId sustain{3'002};
    /// The amplifier envelope's release, in seconds.
    static constexpr ParamId release{3'003};
    /// The attack's curve, from −1 to +1.
    static constexpr ParamId attack_curve{3'004};
    /// The decay's curve, from −1 to +1.
    static constexpr ParamId decay_curve{3'005};
    /// The release's curve, from −1 to +1.
    static constexpr ParamId release_curve{3'006};
    /// What a new note in a note column does to the held voices of this instrument the column
    /// owns: note-off, cut or continue.
    static constexpr ParamId new_note_action{4'000};
};

/// What a new note in a note column does to a held voice the column owns, by the voice's own
/// instrument.
enum class NewNoteAction : std::uint8_t {
    /// Releases it.
    note_off,
    /// Fades it out over 5 ms, as a stolen voice fades, and it leaves the column.
    cut,
    /// Leaves it sounding, still owned by the column.
    carry_on,
};

namespace detail {

/// The labels of the oscillator's waveforms, in the order of their values.
inline constexpr std::array<std::string_view, 4> waveform_labels{
    "Sine",
    "Triangle",
    "Saw",
    "Pulse",
};
/// The labels of the filter's modes, in the order of their values.
inline constexpr std::array<std::string_view, 4> filter_mode_labels{
    "Low-pass",
    "High-pass",
    "Band-pass",
    "Notch",
};
/// The labels of the filter's slopes, in the order of their values.
inline constexpr std::array<std::string_view, 2> filter_slope_labels{
    "12 dB",
    "24 dB",
};
/// The labels of the new note actions, in the order of their values.
inline constexpr std::array<std::string_view, 3> new_note_action_labels{
    "Note-off",
    "Cut",
    "Continue",
};

/// How long a continuous parameter heard while a note sounds takes to reach a new value.
inline constexpr float smoothing_seconds = 0.02F;

/// A continuous parameter's descriptor, smoothed unless `smoothing` says otherwise.
[[nodiscard]] constexpr ParamDescriptor continuous(ParamId id, std::string_view name,
                                                   ParamUnit unit, std::uint8_t precision,
                                                   float min, float max, float default_value,
                                                   float smoothing = smoothing_seconds) {
    return ParamDescriptor{
        .id = id,
        .name = name,
        .unit = unit,
        .precision = precision,
        .min = min,
        .max = max,
        .default_value = default_value,
        .scale = ParamScale::linear,
        .labels = {},
        .smoothing_seconds = smoothing,
    };
}

/// A logarithmic parameter's descriptor.
[[nodiscard]] constexpr ParamDescriptor logarithmic(ParamId id, std::string_view name,
                                                    ParamUnit unit, std::uint8_t precision,
                                                    float min, float max, float default_value,
                                                    float smoothing) {
    ParamDescriptor descriptor =
        continuous(id, name, unit, precision, min, max, default_value, smoothing);
    descriptor.scale = ParamScale::logarithmic;
    return descriptor;
}

/// A stepped parameter's descriptor, which switches at once and isn't modulatable.
[[nodiscard]] constexpr ParamDescriptor stepped(ParamId id, std::string_view name, ParamUnit unit,
                                                float min, float max, float default_value,
                                                std::span<const std::string_view> labels = {}) {
    return ParamDescriptor{
        .id = id,
        .name = name,
        .unit = unit,
        .precision = 0,
        .min = min,
        .max = max,
        .default_value = default_value,
        .scale = ParamScale::stepped,
        .labels = labels,
        .smoothing_seconds = 0.0F,
        .modulatable = false,
    };
}

} // namespace detail

/// The instrument's parameters, in the order of the player's value table.
///
/// Parameters read only when a note or a segment starts, such as the envelope's times, switch at
/// their frame, as stepped ones do. Continuous parameters heard while a note sounds glide over
/// 20 ms.
inline constexpr std::array instrument_params{
    detail::stepped(InstrumentParam::osc1_waveform, "Osc 1 waveform", ParamUnit::none, 0.0F, 3.0F,
                    2.0F, detail::waveform_labels),
    detail::continuous(InstrumentParam::osc1_level, "Osc 1 level", ParamUnit::percent, 1, 0.0F,
                       100.0F, 100.0F),
    detail::continuous(InstrumentParam::osc1_pulse_width, "Osc 1 pulse width", ParamUnit::percent,
                       1, 5.0F, 95.0F, 50.0F),
    detail::stepped(InstrumentParam::osc1_coarse, "Osc 1 coarse", ParamUnit::semitones, -24.0F,
                    24.0F, 0.0F),
    detail::continuous(InstrumentParam::osc1_fine, "Osc 1 fine", ParamUnit::cents, 0, -100.0F,
                       100.0F, 0.0F),
    detail::continuous(InstrumentParam::noise_level, "Noise level", ParamUnit::percent, 1, 0.0F,
                       100.0F, 0.0F),
    detail::stepped(InstrumentParam::filter_mode, "Filter mode", ParamUnit::none, 0.0F, 3.0F, 0.0F,
                    detail::filter_mode_labels),
    detail::stepped(InstrumentParam::filter_slope, "Filter slope", ParamUnit::none, 0.0F, 1.0F,
                    0.0F, detail::filter_slope_labels),
    detail::logarithmic(InstrumentParam::cutoff, "Cutoff", ParamUnit::hertz, 0, 20.0F, 20'000.0F,
                        20'000.0F, detail::smoothing_seconds),
    detail::continuous(InstrumentParam::resonance, "Resonance", ParamUnit::percent, 1, 0.0F, 100.0F,
                       0.0F),
    detail::continuous(InstrumentParam::gain, "Gain", ParamUnit::decibels, 1, -60.0F, 12.0F,
                       -12.0F),
    detail::continuous(InstrumentParam::pan, "Pan", ParamUnit::none, 2, -1.0F, 1.0F, 0.0F),
    detail::continuous(InstrumentParam::velocity_sensitivity, "Velocity sensitivity",
                       ParamUnit::percent, 1, 0.0F, 100.0F, 100.0F, 0.0F),
    detail::logarithmic(InstrumentParam::attack, "Attack", ParamUnit::seconds, 3, 0.001F, 10.0F,
                        0.002F, 0.0F),
    detail::logarithmic(InstrumentParam::decay, "Decay", ParamUnit::seconds, 3, 0.001F, 10.0F, 0.3F,
                        0.0F),
    detail::continuous(InstrumentParam::sustain, "Sustain", ParamUnit::percent, 1, 0.0F, 100.0F,
                       70.0F),
    detail::logarithmic(InstrumentParam::release, "Release", ParamUnit::seconds, 3, 0.001F, 20.0F,
                        0.2F, 0.0F),
    detail::continuous(InstrumentParam::attack_curve, "Attack curve", ParamUnit::none, 2, -1.0F,
                       1.0F, 0.0F, 0.0F),
    detail::continuous(InstrumentParam::decay_curve, "Decay curve", ParamUnit::none, 2, -1.0F, 1.0F,
                       0.5F, 0.0F),
    detail::continuous(InstrumentParam::release_curve, "Release curve", ParamUnit::none, 2, -1.0F,
                       1.0F, 0.5F, 0.0F),
    detail::stepped(InstrumentParam::new_note_action, "New note action", ParamUnit::none, 0.0F,
                    2.0F, 0.0F, detail::new_note_action_labels),
};

/// Returns the pan's gain on each channel, constant-power: `sin(π/2 − θ)` on the left and `sin θ`
/// on the right, with θ from 0 at −1 to π/2 at +1. Both sides take the sine of their angle, so a
/// centred pan, whose angles are both π/4 exactly in float, is the same on each side to the bit.
///
/// \param position The pan, from −1, hard left, to +1, hard right.
/// \return The gain on the left channel, then on the right, each from 0 to 1.
[[nodiscard]] std::pair<float, float> pan_gains(float position);

/// What every voice reads from the instrument during one block: the values it computes once for
/// all of them.
struct InstrumentBlock {
    /// The oscillator's waveform, which holds for the whole block, since a stepped value changes
    /// only at a message's frame, where the player starts a block.
    Waveform waveform = Waveform::saw;
    /// The oscillator's level of each frame, from 0 to 1.
    std::span<const float> osc_level;
    /// The pulse width of each frame, from 0.05 to 0.95 of the period.
    std::span<const float> pulse_width;
    /// The ratio of each frame by which the coarse and fine tuning raise a note's frequency.
    std::span<const float> tuning;
    /// The noise's level of each frame, from 0 to 1.
    std::span<const float> noise_level;
    /// The filter's mode, which holds for the whole block, as the waveform does.
    FilterMode filter_mode = FilterMode::low_pass;
    /// The filter's slope, which holds for the whole block.
    FilterSlope filter_slope = FilterSlope::db12;
    /// What the filter computes with at each frame, from the cutoff and the resonance.
    std::span<const SvfCoefficients> filter;
    /// The amplifier's linear gain of each frame, before the pan, for a voice with a pan of its
    /// own.
    std::span<const float> gain;
    /// The envelope's times and curves, which a segment starting during the block latches.
    AdsrSettings envelope;
    /// The sustain level of each frame, from 0 to 1.
    std::span<const float> sustain;
    /// The gain of each frame on the left channel: the amplifier's gain and the pan's.
    std::span<const float> left_gain;
    /// The gain of each frame on the right channel.
    std::span<const float> right_gain;
};

/// The buffers an instrument computes a block's values into, which a player reuses for each
/// instrument in turn.
struct InstrumentBuffers {
    /// The most frames an instrument computes at once.
    static constexpr std::size_t max_block_frames = 256;

    /// The oscillator's level of each frame.
    std::array<float, max_block_frames> osc_level{};
    /// The pulse width of each frame.
    std::array<float, max_block_frames> pulse_width{};
    /// The tuning of each frame.
    std::array<float, max_block_frames> tuning{};
    /// The noise's level of each frame.
    std::array<float, max_block_frames> noise_level{};
    /// The filter's coefficients of each frame.
    std::array<SvfCoefficients, max_block_frames> filter{};
    /// The amplifier's linear gain of each frame.
    std::array<float, max_block_frames> gain{};
    /// The sustain level of each frame.
    std::array<float, max_block_frames> sustain{};
    /// The gain of each frame on the left channel.
    std::array<float, max_block_frames> left_gain{};
    /// The gain of each frame on the right channel.
    std::array<float, max_block_frames> right_gain{};
};

/// One instrument's live state: the live value of each of its parameters, from which it computes
/// the values that every voice of it reads, once for all of them.
///
/// It holds one `ParamSmoother` for each parameter in `instrument_params`, at the same index, and
/// advances them every frame, whether a voice reads them or not. The amplifier's gain is in
/// decibels, and its pan is constant-power, by `pan_gains()`, so a centred voice is 3 dB down on
/// each side. Both are computed every frame, and so are the tuning, `tuning_ratio(coarse + fine /
/// 100)`, and the filter's coefficients, `svf_coefficients()` of the cutoff and the resonance.
class Instrument {
  public:
    /// The most frames `render()` computes at once.
    static constexpr std::size_t max_block_frames = InstrumentBuffers::max_block_frames;

    /// Makes the instrument with every parameter at its default.
    ///
    /// \param sample_rate The sample rate in hertz, above 0.
    explicit Instrument(float sample_rate);

    /// Sets a parameter's value.
    ///
    /// \param index The parameter's index in `instrument_params`, which is checked.
    /// \param value The new plain value, already constrained to the parameter.
    /// \param smooth Whether it ramps there, as its descriptor says, rather than jump. A player
    ///               ramps only while a voice of the instrument sounds.
    void set(std::size_t index, float value, bool smooth) IRAMA_NONBLOCKING;

    /// \param index The parameter's index in `instrument_params`, which is checked.
    /// \return The parameter's live plain value, as of the latest frame computed.
    [[nodiscard]] float value(std::size_t index) const;

    /// \param index The parameter's index in `instrument_params`, which is checked.
    /// \return The latest plain value the parameter was set to, which it may still be gliding to.
    [[nodiscard]] float target(std::size_t index) const;

    /// \return The sample rate in hertz.
    [[nodiscard]] float sample_rate() const;

    /// \return The oscillator's waveform, as it is now.
    [[nodiscard]] Waveform waveform() const;

    /// \return The filter's mode, as it is now.
    [[nodiscard]] FilterMode filter_mode() const;

    /// \return The filter's slope, as it is now.
    [[nodiscard]] FilterSlope filter_slope() const;

    /// \return The envelope's times and curves, as they are now.
    [[nodiscard]] AdsrSettings envelope() const;

    /// \return What a new note in a note column does to a held voice of this instrument, as it is
    ///         now.
    [[nodiscard]] NewNoteAction new_note_action() const;

    /// Returns the gain of a note played at a velocity, `1 − s · (1 − v²)` for the velocity
    /// sensitivity `s`, from 0 to 1, as it is now.
    ///
    /// \param velocity How hard the note is played, from 0 to 1.
    /// \return The note's gain, from 0 to 1.
    [[nodiscard]] float velocity_gain(float velocity) const;

    /// Computes the next frames' values for the voices, and moves every parameter on past them.
    ///
    /// \param buffers Where to compute the values, overwriting what they hold.
    /// \param frames How many frames, at most `max_block_frames`, which is checked.
    /// \return The values, which stay valid until `buffers` is next written.
    [[nodiscard]] InstrumentBlock render(InstrumentBuffers& buffers,
                                         std::size_t frames) IRAMA_NONBLOCKING;

    /// Moves every parameter on past the next frames without computing their values, as for an
    /// instrument with no voice sounding.
    ///
    /// \param frames How many frames.
    void advance(std::size_t frames) IRAMA_NONBLOCKING;

  private:
    float sample_rate_;
    std::array<ParamSmoother, instrument_params.size()> params_;
};

} // namespace irama
