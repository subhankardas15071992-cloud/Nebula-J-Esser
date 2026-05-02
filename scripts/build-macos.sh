#!/usr/bin/env bash
set -euo pipefail

arch="${1:-arm64}"
config="${2:-Release}"
build_dir="${3:-build/macos-$arch}"
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

cmake -S "$root" -B "$root/$build_dir" -G Ninja \
  -DCMAKE_BUILD_TYPE="$config" \
  -DCMAKE_OSX_ARCHITECTURES="$arch" \
  -DNJ_BUILD_TESTS=ON \
  -DNJ_ENABLE_CLAP=ON

cmake --build "$root/$build_dir" --parallel
ctest --test-dir "$root/$build_dir" --output-on-failure
