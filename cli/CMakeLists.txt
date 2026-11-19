# SPDX-FileCopyrightText: 2026 takumu-dev
# SPDX-License-Identifier: GPL-3.0-or-later

# irama-cli, a client of libirama's internal C++ API. It takes the release version from the
# project, since irama.h belongs to the C API, which it doesn't link.
add_executable(irama-cli main.cpp)
target_link_libraries(irama-cli PRIVATE irama::engine)
target_compile_definitions(irama-cli PRIVATE IRAMA_VERSION_STRING="${PROJECT_VERSION}")
irama_target_conventions(irama-cli)
