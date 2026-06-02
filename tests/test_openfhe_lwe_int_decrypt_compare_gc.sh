#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

out_dir="$(mktemp -d)"
trap 'rm -rf "$out_dir"' EXIT
key_dir="$out_dir/openfhe_keypair"
material="$out_dir/v2_lwe_integer_material.txt"

cmake -S . -B build-local -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build build-local \
    --target generate_openfhe_keypair \
    --target export_openfhe_lwe_int_material \
    --target openfhe_lwe_int_decrypt_compare_gc_test \
    --parallel 2 >/dev/null

./build-local/generate_openfhe_keypair "$key_dir" >/dev/null

./build-local/export_openfhe_lwe_int_material \
    "$key_dir" \
    "$material" >/dev/null

./build-local/openfhe_lwe_int_decrypt_compare_gc_test "$material"
