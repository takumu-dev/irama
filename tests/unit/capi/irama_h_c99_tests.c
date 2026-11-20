// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

// Built as C99 with every warning an error: if irama.h strays from C99, this fails to compile.
#include <irama/irama.h>

int main(void) { return irama_version() == IRAMA_VERSION ? 0 : 1; }
