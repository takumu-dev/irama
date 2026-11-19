// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include <irama/irama.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("versions encode as major, minor and patch in thousands", "[capi]") {
    STATIC_REQUIRE(IRAMA_MAKE_VERSION(1, 2, 3) == 1002003);
    STATIC_REQUIRE(IRAMA_MAKE_VERSION(0, 999, 999) < IRAMA_MAKE_VERSION(1, 0, 0));
}

TEST_CASE("irama_version reports the version irama.h declares", "[capi]") {
    REQUIRE(irama_version() == IRAMA_VERSION);
}

TEST_CASE("irama.h declares the version the build was configured with", "[capi]") {
    STATIC_REQUIRE(IRAMA_VERSION == IRAMA_MAKE_VERSION(IRAMA_TEST_PROJECT_VERSION_MAJOR,
                                                       IRAMA_TEST_PROJECT_VERSION_MINOR,
                                                       IRAMA_TEST_PROJECT_VERSION_PATCH));
}
