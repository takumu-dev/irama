// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

/// \file
/// The writer of float32 WAV files.

#pragma once

#include <cstdint>
#include <cstdio>
#include <expected>
#include <memory>
#include <span>

namespace irama {

/// The layout of the samples in a WAV file.
struct WavFormat {
    /// The frames a second.
    std::uint32_t sample_rate = 48'000;
    /// The samples in each frame, one for each channel, as wide as the WAV header's own field.
    std::uint16_t channels = 2;
};

/// Why a WAV file couldn't be written.
enum class WavError : std::uint8_t {
    /// The file couldn't be opened for writing.
    open_failed,
    /// Writing to the file, or closing it, failed.
    write_failed,
    /// The samples would make the file larger than a WAV file can describe, 4 GiB.
    too_long,
};

/// Writes interleaved 32-bit float samples to a WAV file.
///
/// The file is IEEE float, as WAV's `WAVE_FORMAT_IEEE_FLOAT` describes it, with a `fact` chunk
/// holding its frame count. The writer writes through `<cstdio>` and allocates nothing itself.
/// Its header gives the file's length, which is known only at the end, so a file is complete only
/// once `finish()` has succeeded. A writer destroyed before then closes its file unfinished.
class WavWriter {
  public:
    /// Opens the file at `path` for writing, replacing any file there, and writes a header for no
    /// samples yet.
    ///
    /// \param path The file to write, which must not be null.
    /// \param format The layout of the samples, with a rate and a channel count above 0.
    /// \return The writer, or `WavError::open_failed` if the file couldn't be opened or its header
    ///     written.
    [[nodiscard]] static std::expected<WavWriter, WavError> create(const char* path,
                                                                   WavFormat format);

    /// Appends samples to the file.
    ///
    /// \param samples Whole frames of interleaved samples, one for each channel in turn.
    /// \return Nothing, or `WavError::too_long` if the file can't hold the samples, in which case
    ///     none of them is written, or `WavError::write_failed`.
    [[nodiscard]] std::expected<void, WavError> write(std::span<const float> samples);

    /// Writes the file's length into its header and closes it. The writer then writes no more.
    ///
    /// \return Nothing, or `WavError::write_failed`.
    [[nodiscard]] std::expected<void, WavError> finish();

  private:
    struct Closer {
        void operator()(std::FILE* file) const;
    };

    WavWriter(std::FILE* file, WavFormat format);

    std::unique_ptr<std::FILE, Closer> file_;
    WavFormat format_;
    std::uint32_t data_bytes_ = 0;
};

} // namespace irama
