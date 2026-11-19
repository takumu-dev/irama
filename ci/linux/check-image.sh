#!/bin/sh
# SPDX-FileCopyrightText: 2026 takumu-dev
# SPDX-License-Identifier: Zlib

# Checks that the image keeps the promises CI relies on: the compilers at their floors under the
# names the presets use, GCC 16's libstdc++ under both of them, the sanitizer runtimes, LLVM's
# format and tidy tools, and what CMake, vcpkg and CircleCI call. The image's build runs this
# last, so an image that breaks a promise is never pushed. It reports every broken promise, not
# just the first.
set -u

failures=0

fail() {
    echo "check-image: $*" >&2
    failures=$((failures + 1))
}

# Runs a command and expects its output to match an extended regular expression.
expect() {
    what=$1
    pattern=$2
    shift 2
    if ! output=$("$@" 2>&1); then
        fail "$what: \`$*\` failed: $output"
    elif ! printf '%s\n' "$output" | grep -Eq -- "$pattern"; then
        fail "$what: \`$*\` printed: $output"
    fi
}

expect_command() {
    for name in "$@"; do
        command -v "$name" > /dev/null || fail "$name isn't on the PATH"
    done
}

# Builds the program with the given compiler and flags and, unless told not to, runs it. Every
# libstdc++ it loads must be GCC 16's, from /usr/local/lib64.
expect_program() {
    what=$1
    run=$2
    shift 2
    program="$work/$(printf '%s' "$what" | tr -c '[:alnum:]' '_')"
    if ! output=$("$@" -std=c++23 -o "$program" "$work/program.cpp" 2>&1); then
        fail "$what: \`$*\` failed to build the program: $output"
        return
    fi
    if [ "$run" = run ]; then
        expect "$what" '^42$' "$program"
    fi
    for library in $(ldd "$program" | awk '$1 ~ /^libstdc\+\+/ && $2 == "=>" { print $3 }'); do
        library=$(readlink -f "$library")
        case $library in
            /usr/local/lib64/*) ;;
            *) fail "$what: the program loads $library, not GCC 16's libstdc++" ;;
        esac
    done
}

work=$(mktemp -d) || exit 1
trap 'rm -rf "$work"' EXIT

# The floor compilers, under their plain names, since the presets and vcpkg name them so.
for compiler in gcc g++ cc c++; do
    expect "$compiler is GCC 16.2" '^16\.2\.' "$compiler" -dumpfullversion
done
expect "clang is Clang 23" 'clang version 23\.' clang --version
expect "clang++ is Clang 23" 'clang version 23\.' clang++ --version
expect "clang-tidy is LLVM 23's" 'LLVM version 23\.' clang-tidy --version
expect "clang-format is LLVM 23's" 'clang-format version 23\.' clang-format --version

# libstdc++ 16, built against and loaded at run time. Clang must take GCC 16's, not Debian's.
cat > "$work/program.cpp" << 'EOF' || exit 1
#include <print>
#include <version>

static_assert(_GLIBCXX_RELEASE == 16, "libstdc++ is GCC 16's");

int main() { std::println("{}", 42); }
EOF
expect_program "GCC with libstdc++" run g++
expect_program "Clang with libstdc++" run clang++
expect_program "Clang with AddressSanitizer and UndefinedBehaviorSanitizer" run \
    clang++ -fsanitize=address,undefined -fno-sanitize-recover=all
expect_program "Clang with RealtimeSanitizer" run clang++ -fsanitize=realtime
# ThreadSanitizer won't start under the ASLR entropy of most hosts, this build's included, so the
# program is only built.
expect_program "Clang with ThreadSanitizer" build-only clang++ -fsanitize=thread

# The build tools the presets name, and what vcpkg and the CI steps call. CircleCI's checkout step
# clones over SSH.
expect "CMake is 4.4" '^cmake version 4\.4\.' cmake --version
expect_command ninja git ssh curl tar zip unzip xz pkg-config jq

if [ "$failures" -ne 0 ]; then
    echo "check-image: promises broken: $failures" >&2
    exit 1
fi
echo "check-image: every promise holds"
