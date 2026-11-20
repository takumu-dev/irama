# SPDX-FileCopyrightText: 2026 takumu-dev
# SPDX-License-Identifier: Zlib

# The image every Linux job of CI runs in, published as ghcr.io/takumu-dev/irama-ci with a tag
# named by the hash of this directory (see ci/image-tag.sh). A tag is built once and never rebuilt,
# so any change here makes a new image.
#
# It starts from Docker's official GCC image, which holds GCC 16 in /usr/local on Debian trixie.
# Clang comes from apt.llvm.org and is pointed at GCC 16's libstdc++, CMake comes from Kitware, and
# Doxygen from its own releases, since Debian's is too old to warn as the floor does. Distro
# packages are read as they stand when the image is built.

FROM gcc:16.2.0-trixie@sha256:5c2f83335c97e233c80d2bc754bd5c46b2d9c32f236a765a4ac40a59f216c064

LABEL org.opencontainers.image.source=https://github.com/takumu-dev/irama
LABEL org.opencontainers.image.description="Irama's Linux CI: GCC 16, Clang 23 on libstdc++ 16, CMake 4.4 and Doxygen 1.18"

ARG DEBIAN_FRONTEND=noninteractive
ARG CMAKE_VERSION=4.4.5
ARG CMAKE_SHA256=ed85135c5a5135b45e50423cb406dea04ca3338da0b8ef3c9ede963559680be0
ARG DOXYGEN_VERSION=1.18.0
ARG DOXYGEN_SHA256=14fa81bdc34171edb5f1f02b1d60e74802f0439b77fa44e592565d517d72df90
ARG LLVM_KEY_FINGERPRINT=6084F3CF814B57C1CF12EFD515CF4D18AF4F7421

# apt.llvm.org's signing key, which apt trusts only once it is checked to be the one key with that
# fingerprint.
RUN curl --fail --silent --show-error --location --retry 3 \
        --output /etc/apt/keyrings/apt.llvm.org.asc https://apt.llvm.org/llvm-snapshot.gpg.key \
    && gpg --show-keys --with-colons /etc/apt/keyrings/apt.llvm.org.asc > /tmp/llvm-key \
    && [ "$(grep -c '^pub:' /tmp/llvm-key)" = 1 ] \
    && [ "$(awk -F: '$1 == "fpr" { print $10; exit }' /tmp/llvm-key)" = "${LLVM_KEY_FINGERPRINT}" ] \
    && rm -rf /tmp/llvm-key /root/.gnupg
COPY apt.llvm.org.sources /etc/apt/sources.list.d/

#   Clang 23 and its tools, with the sanitizer runtimes: clang-23 to libclang-rt-23-dev
#   the presets' generator: ninja-build
#   vcpkg and the CI steps: ca-certificates, curl, git, jq, pkgconf, tar, unzip, xz-utils, zip
RUN apt-get update \
    && apt-get install --yes --no-install-recommends \
        clang-23 clang-format-23 clang-tidy-23 libclang-rt-23-dev \
        ninja-build \
        ca-certificates curl git jq pkgconf tar unzip xz-utils zip \
    && rm -rf /var/lib/apt/lists/*

RUN curl --fail --silent --show-error --location --retry 3 --output /tmp/cmake.tar.gz \
        "https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-linux-x86_64.tar.gz" \
    && echo "${CMAKE_SHA256}  /tmp/cmake.tar.gz" | sha256sum --check --strict \
    && mkdir /opt/cmake \
    && tar --extract --gzip --file /tmp/cmake.tar.gz --directory /opt/cmake --strip-components 1 \
    && rm /tmp/cmake.tar.gz

# Upstream names each release's tag after its version, with underscores: Release_1_18_0.
RUN curl --fail --silent --show-error --location --retry 3 --output /tmp/doxygen.tar.gz \
        "https://github.com/doxygen/doxygen/releases/download/Release_$(echo "${DOXYGEN_VERSION}" | tr . _)/doxygen-${DOXYGEN_VERSION}.linux.bin.tar.gz" \
    && echo "${DOXYGEN_SHA256}  /tmp/doxygen.tar.gz" | sha256sum --check --strict \
    && mkdir /opt/doxygen \
    && tar --extract --gzip --file /tmp/doxygen.tar.gz --directory /opt/doxygen \
        --strip-components 1 \
    && rm /tmp/doxygen.tar.gz

COPY clang.cfg /usr/lib/llvm-23/bin/clang.cfg
COPY clang.cfg /usr/lib/llvm-23/bin/clang++.cfg

# Plain compiler and tool names, as the presets use them: GCC 16 first, then CMake, then LLVM 23,
# then Doxygen. The base image already loads /usr/local/lib64, GCC 16's libstdc++, ahead of Debian's
# at run time.
ENV PATH=/usr/local/bin:/opt/cmake/bin:/usr/lib/llvm-23/bin:/opt/doxygen/bin:/usr/local/sbin:/usr/sbin:/usr/bin:/sbin:/bin
ENV LANG=C.UTF-8

COPY check-image.sh /usr/local/bin/check-image
RUN check-image
