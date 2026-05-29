#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

tmp_dir="$(mktemp -d)"
trap 'rm -rf "$tmp_dir"' EXIT

material="$tmp_dir/v2_lwe_integer_material.txt"
runtime_dir="$tmp_dir/runtime"
output="$tmp_dir/build_mock_v2_runtime_output.txt"

cmake -S . -B build-local -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build build-local \
    --target export_openfhe_lwe_int_material \
    --parallel 2 >/dev/null

./build-local/export_openfhe_lwe_int_material \
    demo_keys/openfhe_binfhe_demo_keypair \
    "$material" >/dev/null

bash build_mock.sh --v2-openfhe-runtime-demo "$material" "$runtime_dir" \
    >"$output"

grep -q "Running v2 OpenFHE LWE integer runtime demo" "$output"
grep -q "OpenFHE LWE integer runtime boundary audit passed" "$output"
grep -q "GC_f(x', b') = GC{ \\[Dec_hsk(x') <= Dec_hsk(b')\\] }" "$output"
grep -q "Predicate sequence: 1,1,1,1,1,0" "$output"
grep -q "Encrypted loop iterations executed: 5" "$output"
