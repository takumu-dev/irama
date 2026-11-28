# SPDX-FileCopyrightText: 2026 takumu-dev
# SPDX-License-Identifier: Zlib

# libirama: one static library per component under src/, each added with irama_add_component().

# The internal C++ API, for irama-cli: every component but capi.
add_library(irama_engine INTERFACE)
add_library(irama::engine ALIAS irama_engine)

add_subdirectory(src/base)
add_subdirectory(src/capi)
add_subdirectory(src/wav)
add_subdirectory(src/param)
add_subdirectory(src/source)
add_subdirectory(src/filter)
add_subdirectory(src/voice)
add_subdirectory(src/model)
add_subdirectory(src/seq)
add_subdirectory(src/player)

# The imported targets that libirama may link: those of its own dependencies, at the top level of
# vcpkg.json, and nothing else. It has none. Once the whole project is configured, any other link
# from a libirama target stops the configuration.
set(irama_libirama_imports "")
irama_check_link_closure(${irama_libirama_imports})
