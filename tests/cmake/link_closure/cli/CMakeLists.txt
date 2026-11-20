# SPDX-FileCopyrightText: 2026 takumu-dev
# SPDX-License-Identifier: Zlib

if(CASE STREQUAL "late")
    target_link_libraries(irama_engine INTERFACE Fake::forbidden)
endif()
