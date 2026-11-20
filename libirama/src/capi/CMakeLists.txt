# SPDX-FileCopyrightText: 2026 takumu-dev
# SPDX-License-Identifier: Zlib

# The C API that games call. Its public include tree holds irama.h alone.
irama_add_component(capi C_API
    INCLUDE_DIR "${PROJECT_SOURCE_DIR}/libirama/include"
    HEADERS "${PROJECT_SOURCE_DIR}/libirama/include/irama/irama.h"
    SOURCES version.cpp
    DEPENDS base
)
