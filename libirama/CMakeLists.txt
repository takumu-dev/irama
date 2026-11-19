# SPDX-FileCopyrightText: 2026 takumu-dev
# SPDX-License-Identifier: Zlib

# libirama: one static library per component under src/, each added with irama_add_component().

# The internal C++ API, for irama-cli: every component but capi.
add_library(irama_engine INTERFACE)
add_library(irama::engine ALIAS irama_engine)

add_subdirectory(src/capi)
