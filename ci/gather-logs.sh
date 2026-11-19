#!/bin/sh
# SPDX-FileCopyrightText: 2026 takumu-dev
# SPDX-License-Identifier: Zlib

# Copies the logs a failed job leaves into a directory, for CircleCI to keep, under their paths in
# the checkout: each build's workflow output, configure log, test logs, JUnit file, vcpkg install
# log and the output of failed golden tests. vcpkg's logs of the ports it built go under vcpkg/.
#
#   ci/gather-logs.sh logs
set -eu

destination=$1
for path in build/*/workflow.log build/*/CMakeFiles/CMakeConfigureLog.yaml build/*/Testing/Temporary \
    build/*/junit.xml build/*/vcpkg-manifest-install.log build/*/golden-failures; do
    if [ -e "$path" ]; then
        mkdir -p "$destination/$(dirname "$path")"
        cp -R "$path" "$destination/$(dirname "$path")/"
    fi
done
for log in "${VCPKG_ROOT:-/nonexistent}"/buildtrees/*/*.log; do
    if [ -e "$log" ]; then
        port=$(basename "$(dirname "$log")")
        mkdir -p "$destination/vcpkg/$port"
        cp "$log" "$destination/vcpkg/$port/"
    fi
done
