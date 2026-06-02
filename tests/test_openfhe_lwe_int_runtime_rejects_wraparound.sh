#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

tmp_dir="$(mktemp -d)"
trap 'rm -rf "$tmp_dir"' EXIT

key_dir="$tmp_dir/openfhe_keypair"
material="$tmp_dir/wraparound_material.txt"
runtime_dir="$tmp_dir/runtime"
setup_output="$tmp_dir/setup_output.txt"

cmake -S . -B build-local -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build build-local \
    --target generate_openfhe_keypair \
    --target export_openfhe_lwe_int_material \
    --target setup_openfhe_lwe_int_gc_material \
    --parallel 2 >/dev/null

./build-local/generate_openfhe_keypair "$key_dir" >/dev/null

./build-local/export_openfhe_lwe_int_material \
    "$key_dir" \
    "$material" \
    15 \
    15 \
    1 >/dev/null

if ./build-local/setup_openfhe_lwe_int_gc_material \
    "$material" \
    "$runtime_dir" >"$setup_output" 2>&1; then
    echo "setup accepted wraparound-unsafe 15..15 demo material" >&2
    exit 1
fi

grep -q "wraparound-unsafe" "$setup_output"
