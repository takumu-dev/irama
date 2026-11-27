// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

/// \file
/// The state variable filter.

#pragma once

#include <irama/engine/base/nonblocking.hpp>

#include <cstdint>
#include <span>

namespace irama {

/// The modes of an `Svf`, in the order of the filter mode parameter's values.
enum class FilterMode : std::uint8_t {
    /// Passes what is below the cutoff.
    low_pass,
    /// Passes what is above the cutoff.
    high_pass,
    /// Passes what is around the cutoff, at a gain of 1 at the cutoff itself.
    band_pass,
    /// Passes everything but what is around the cutoff.
    notch,
};

/// The slopes of an `Svf`, in the order of the filter slope parameter's values.
enum class FilterSlope : std::uint8_t {
    /// 12 dB per octave: one two-pole stage.
    db12,
    /// 24 dB per octave: two two-pole stages in series.
    db24,
};

/// What one two-pole stage of an `Svf` computes with, for one sample: its damping `2R`, which is
/// `1 / Q`, and the two values derived from it and `g`.
struct SvfStage {
    /// `2R`, the damping, `1 / Q`.
    float damping = 0.0F;
    /// `2R + g`, which scales the first integrator's state in the feedback.
    float feedback = 0.0F;
    /// `1 / (1 + 2R·g + g²)`, which solves the zero-delay feedback loop.
    float scale = 0.0F;

    /// \return Whether every value is the same as another stage's.
    [[nodiscard]] bool operator==(const SvfStage&) const = default;
};

/// What an `Svf` computes with for one sample, at one cutoff and one resonance: `g`, the
/// integrators' prewarped gain, and each stage's values.
struct SvfCoefficients {
    /// `tan(π · fc / fs)`.
    float g = 0.0F;
    /// The first stage of the 24 dB slope, Butterworth-damped, with Q = 1/√2.
    SvfStage butterworth;
    /// The stage that carries the resonance: the only stage of the 12 dB slope, and the second of
    /// the 24 dB one.
    SvfStage resonant;

    /// \return Whether every value is the same as other coefficients'.
    [[nodiscard]] bool operator==(const SvfCoefficients&) const = default;
};

/// The highest cutoff an `Svf` filters at, as a fraction of the sample rate.
inline constexpr float max_cutoff_ratio = 0.45F;

/// Returns what an `Svf` computes with at a cutoff and a resonance.
///
/// The cutoff is held at most at `max_cutoff_ratio` of the sample rate, and the resonance `r` gives
/// the resonant stage `Q = 0.5 · 50^r`, from 0.5 at 0 to 25 at 1, so the filter never
/// self-oscillates.
///
/// \param cutoff The cutoff in hertz, above 0, which is checked.
/// \param resonance The resonance, from 0 to 1, which is checked.
/// \param sample_rate The sample rate in hertz, above 0.
/// \return The coefficients.
[[nodiscard]] SvfCoefficients svf_coefficients(float cutoff, float resonance, float sample_rate);

/// A linear state variable filter in the topology-preserving transform, with low-pass, high-pass,
/// band-pass and notch modes at 12 and 24 dB per octave.
///
/// Each stage is Zavalishin's two-pole state variable filter with trapezoidal integrators, whose
/// response is the analogue one's under the bilinear transform prewarped at the cutoff. With `s1`
/// and `s2` the integrators' states, one sample `x` is:
///
///     high = (x − (2R + g)·s1 − s2) / (1 + 2R·g + g²)
///     band = g·high + s1,  s1 = band + g·high
///     low  = g·band + s2,  s2 = low + g·band
///
/// The low-pass, high-pass and notch outputs are `low`, `high` and `low + high`, and the band-pass
/// output is `2R·band`, normalised to a gain of 1 at the cutoff. The 24 dB slope is two stages in
/// series in the same mode at the same cutoff: the first damped as a Butterworth filter, and only
/// the second carrying the resonance, so there is one resonant peak. The 12 dB slope uses only the
/// resonant stage, and the other holds no state, so that a change to 24 dB starts it from silence.
///
/// **Denormals.** At the end of each `render()` call, each state below 1e-20 in magnitude, 400 dB
/// below full scale, is set to 0, so that a filter left ringing into silence settles to exact
/// zeros rather than denormals, whatever the floating-point environment.
class Svf {
  public:
    /// Clears the filter's state, as if it had only ever filtered silence.
    void reset();

    /// Filters samples in place.
    ///
    /// \param samples The samples to filter, overwritten with the filter's output.
    /// \param mode The mode to filter in.
    /// \param slope The slope to filter at.
    /// \param coefficients What to compute with for each sample, at least as many.
    void render(std::span<float> samples, FilterMode mode, FilterSlope slope,
                std::span<const SvfCoefficients> coefficients) IRAMA_NONBLOCKING;

  private:
    // The two integrators' states of one stage.
    struct State {
        float s1 = 0.0F;
        float s2 = 0.0F;
    };

    State butterworth_;
    State resonant_;
};

} // namespace irama
