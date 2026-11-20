#!/bin/sh
# SPDX-FileCopyrightText: 2026 takumu-dev
# SPDX-License-Identifier: Zlib

# Sets up vcpkg in VCPKG_ROOT at the baseline vcpkg.json names, and makes the binary cache in
# VCPKG_DEFAULT_BINARY_CACHE. Run it from the root of the checkout.
#
# vcpkg reads the ports at the versions the manifest names from the registry's history, so a shallow
# clone won't do. A blobless one fetches only the files vcpkg reads.
#
#   ci/setup-vcpkg.sh
set -eu

baseline=$(jq --raw-output '."builtin-baseline"' vcpkg.json)
git clone --quiet --filter=blob:none --no-checkout https://github.com/microsoft/vcpkg "$VCPKG_ROOT"
git -C "$VCPKG_ROOT" checkout --quiet "$baseline"
"$VCPKG_ROOT/bootstrap-vcpkg.sh" -disableMetrics
mkdir -p "$VCPKG_DEFAULT_BINARY_CACHE"
