// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

/// \file
/// The spectrum of a signal, and how much of it isn't harmonic.

#pragma once

#include <complex>
#include <cstddef>
#include <span>
#include <vector>

namespace irama_test_support {

/// Transforms `values` in place into their discrete Fourier transform, `X[k] = Σ x[n] ·
/// e^(−2πi·k·n/N)`, with a radix-2 decimation-in-time FFT in double precision.
///
/// \param values The signal, whose length is a power of two, or the program writes a report to
///               stderr and aborts.
void fft(std::vector<std::complex<double>>& values);

/// Returns the discrete Fourier transform of `values` by the direct sum, in `O(N²)`, against which
/// `fft()` is checked.
///
/// \param values The signal, of any length.
/// \return Its transform.
[[nodiscard]] std::vector<std::complex<double>>
dft(const std::vector<std::complex<double>>& values);

/// Returns the seven-term Blackman-Harris window of a length, in its periodic form for spectral
/// analysis: `w[n] = Σ (−1)^k · a_k · cos(2πkn/N)` for `k` from 0 to 6, with Albrecht's
/// coefficients. Its sidelobes lie far below anything a `float` signal can resolve, so a component
/// 140 dB down still shows, and its main lobe is 7 bins wide on each side.
///
/// \param size The window's length, above 0.
/// \return The window.
[[nodiscard]] std::vector<double> blackman_harris(std::size_t size);

/// Returns how strong the strongest component of a periodic signal that isn't one of its harmonics
/// is, up to a frequency, in decibels relative to its fundamental: the measure of an oscillator's
/// aliasing.
///
/// The signal is windowed with `blackman_harris()` and transformed with `fft()`. Each component is
/// the peak of the magnitude spectrum, the fundamental's being the highest bin within the main lobe
/// around its frequency. A bin within the main lobe of a harmonic, any whole multiple of the
/// fundamental, DC among them, belongs to that harmonic, and every other bin up to `highest` is a
/// candidate.
///
/// \param signal The signal, whose length is a power of two, or the program writes a report to
///               stderr and aborts.
/// \param fundamental The fundamental's frequency in hertz, above 0.
/// \param sample_rate The sample rate in hertz.
/// \param highest The highest frequency whose components count, in hertz, up to half the sample
///                rate.
/// \return The strongest non-harmonic component, in decibels relative to the fundamental.
[[nodiscard]] double strongest_non_harmonic(std::span<const float> signal, double fundamental,
                                            double sample_rate, double highest);

} // namespace irama_test_support
