// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include <irama/test_support/spectrum.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <numbers>
#include <span>
#include <utility>
#include <vector>

namespace {

// Albrecht's coefficients of the seven-term Blackman-Harris window.
constexpr std::array<double, 7> window_terms{
    // NOLINTNEXTLINE(modernize-use-std-numbers): a coefficient that only looks like log10(e).
    0.27105140069342, 0.43329793923448, 0.21812299954311, 0.06592544638803,
    0.01081174209837, 0.00077658482522, 0.00001388721735,
};

// How many bins on each side of a component's frequency its main lobe covers in the window.
constexpr double main_lobe_bins = 7.0;

// Writes a report and aborts unless `size` is a power of two, as a failed assertion would.
void require_power_of_two(std::size_t size) {
    if (!std::has_single_bit(size)) {
        std::fputs("irama_test_support: a transformed signal's length is a power of two\n", stderr);
        std::abort();
    }
}

} // namespace

namespace irama_test_support {

void fft(std::vector<std::complex<double>>& values) {
    const std::size_t size = values.size();
    require_power_of_two(size);

    // Each value moves to the index whose bits are its own reversed.
    for (std::size_t index = 1, reversed = 0; index < size; ++index) {
        std::size_t bit = size >> 1U;
        for (; (reversed & bit) != 0; bit >>= 1U) {
            reversed ^= bit;
        }
        reversed ^= bit;
        if (index < reversed) {
            std::swap(values.at(index), values.at(reversed));
        }
    }

    // Each twiddle factor computed directly, rather than by repeated multiplication, so that the
    // rounding doesn't grow with the length.
    std::vector<std::complex<double>> twiddles(size / 2);
    for (std::size_t index = 0; index < twiddles.size(); ++index) {
        twiddles.at(index) = std::polar(1.0, -2.0 * std::numbers::pi * static_cast<double>(index) /
                                                 static_cast<double>(size));
    }

    for (std::size_t length = 2; length <= size; length <<= 1U) {
        const std::size_t half = length / 2;
        const std::size_t stride = size / length;
        for (std::size_t start = 0; start < size; start += length) {
            for (std::size_t offset = 0; offset < half; ++offset) {
                const std::complex<double> even = values.at(start + offset);
                const std::complex<double> odd =
                    values.at(start + offset + half) * twiddles.at(offset * stride);
                values.at(start + offset) = even + odd;
                values.at(start + offset + half) = even - odd;
            }
        }
    }
}

std::vector<std::complex<double>> dft(const std::vector<std::complex<double>>& values) {
    const std::size_t size = values.size();
    std::vector<std::complex<double>> transformed(size);
    for (std::size_t bin = 0; bin < size; ++bin) {
        for (std::size_t index = 0; index < size; ++index) {
            // The product reduced modulo the length first, so that the angle stays small.
            const auto turns =
                static_cast<double>((bin * index) % size) / static_cast<double>(size);
            transformed.at(bin) +=
                values.at(index) * std::polar(1.0, -2.0 * std::numbers::pi * turns);
        }
    }
    return transformed;
}

std::vector<double> blackman_harris(std::size_t size) {
    std::vector<double> window(size);
    for (std::size_t index = 0; index < size; ++index) {
        const double angle =
            2.0 * std::numbers::pi * static_cast<double>(index) / static_cast<double>(size);
        double sign = 1.0;
        for (std::size_t term = 0; term < window_terms.size(); ++term) {
            window.at(index) +=
                sign * window_terms.at(term) * std::cos(static_cast<double>(term) * angle);
            sign = -sign;
        }
    }
    return window;
}

double strongest_non_harmonic(std::span<const float> signal, double fundamental, double sample_rate,
                              double highest) {
    const std::size_t size = signal.size();
    require_power_of_two(size);

    const auto window = blackman_harris(size);
    std::vector<std::complex<double>> spectrum(size);
    std::ranges::transform(signal, window, spectrum.begin(), [](float sample, double weight) {
        return std::complex<double>{static_cast<double>(sample) * weight};
    });
    fft(spectrum);

    const std::size_t nyquist_bin = size / 2;
    const auto highest_bin = std::min(
        nyquist_bin, static_cast<std::size_t>(highest * static_cast<double>(size) / sample_rate));
    std::vector<double> magnitudes(nyquist_bin + 1);
    std::ranges::transform(std::span{spectrum}.first(nyquist_bin + 1), magnitudes.begin(),
                           [](std::complex<double> value) { return std::abs(value); });

    // Where the fundamental and its harmonics fall, in bins, which needn't be whole.
    const double spacing = fundamental * static_cast<double>(size) / sample_rate;
    const auto near_harmonic = [&](std::size_t bin) {
        const double position = static_cast<double>(bin) / spacing;
        const double nearest = std::round(position);
        return std::abs(position - nearest) * spacing <= main_lobe_bins;
    };

    double fundamental_peak = 0.0;
    double strongest = 0.0;
    for (std::size_t bin = 0; bin <= nyquist_bin; ++bin) {
        const double magnitude = magnitudes.at(bin);
        if (std::abs(static_cast<double>(bin) - spacing) <= main_lobe_bins) {
            fundamental_peak = std::max(fundamental_peak, magnitude);
        }
        if (bin <= highest_bin && !near_harmonic(bin)) {
            strongest = std::max(strongest, magnitude);
        }
    }
    return 20.0 * std::log10(strongest / fundamental_peak);
}

} // namespace irama_test_support
