# SPDX-FileCopyrightText: 2026 takumu-dev
# SPDX-License-Identifier: Zlib

# The WAV writer that irama-cli and the tests share.
irama_add_component(wav
    HEADERS include/irama/engine/wav/wav_writer.hpp
    SOURCES wav_writer.cpp
    DEPENDS base
)
# The writer opens its file with std::fopen, which MSVC deprecates in favour of its own fopen_s.
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_definitions(irama_wav PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
