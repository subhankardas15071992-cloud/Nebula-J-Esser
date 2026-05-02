#!/usr/bin/env bash
set -euo pipefail

config="${1:-Release}"
build_dir="${2:-build/linux-x64}"
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

cmake -S "$root" -B "$root/$build_dir" -G Ninja \
  -DCMAKE_BUILD_TYPE="$config" \
  -DNJ_BUILD_TESTS=ON \
  -DNJ_ENABLE_CLAP=ON

cmake --build "$root/$build_dir" --parallel
ctest --test-dir "$root/$build_dir" --output-on-failure
