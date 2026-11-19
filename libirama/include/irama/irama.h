// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

/// \file
/// The C API of libirama, the library that plays Irama songs.
///
/// This header compiles as C99 and as C++11 or newer.

#ifndef IRAMA_IRAMA_H
#define IRAMA_IRAMA_H

/// The major part of the version of libirama this header belongs to. It changes when a release
/// breaks code written against an earlier one.
#define IRAMA_VERSION_MAJOR 0

/// The minor part of the version of libirama this header belongs to. It changes when a release
/// adds to the API without breaking it.
#define IRAMA_VERSION_MINOR 0

/// The patch part of the version of libirama this header belongs to. It changes when a release
/// only fixes bugs.
#define IRAMA_VERSION_PATCH 0

/// Encodes a version as one integer, `major * 1000000 + minor * 1000 + patch`, so that versions
/// compare in order as plain numbers: 1.2.3 is `1002003`.
///
/// \param major The major part of the version.
/// \param minor The minor part of the version, below 1000.
/// \param patch The patch part of the version, below 1000.
#define IRAMA_MAKE_VERSION(major, minor, patch) (((major) * 1000000) + ((minor) * 1000) + (patch))

/// The version of libirama this header belongs to, encoded by IRAMA_MAKE_VERSION(). Compare it
/// with irama_version() to learn whether the library a program runs with is the one it was
/// compiled against.
#define IRAMA_VERSION                                                                              \
    IRAMA_MAKE_VERSION(IRAMA_VERSION_MAJOR, IRAMA_VERSION_MINOR, IRAMA_VERSION_PATCH)

#ifdef __cplusplus
extern "C" {
#endif

/// Returns the version of the libirama the program runs with, encoded by IRAMA_MAKE_VERSION().
/// It differs from IRAMA_VERSION if the program was compiled against another version's header.
///
/// Any thread may call it, the audio thread included, at any time. It allocates nothing.
///
/// \return The library's version: `1002003` for 1.2.3.
int irama_version(void);

#ifdef __cplusplus
}
#endif

#endif
