#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

tmp_dir="$(mktemp -d)"
trap 'rm -rf "$tmp_dir"' EXIT

key_dir="$tmp_dir/openfhe_keypair"
material="$tmp_dir/v2_lwe_integer_material.txt"
runtime_dir="$tmp_dir/runtime"
output="$tmp_dir/build_mock_v2_runtime_output.txt"

cmake -S . -B build-local -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build build-local \
    --target generate_openfhe_keypair \
    --target export_openfhe_lwe_int_material \
    --parallel 2 >/dev/null

./build-local/generate_openfhe_keypair "$key_dir" >/dev/null

for attempt in 1 2 3 4 5; do
    rm -rf "$runtime_dir"
    ./build-local/export_openfhe_lwe_int_material \
        "$key_dir" \
        "$material" >/dev/null

    if bash build_mock.sh --v2-openfhe-runtime-demo "$material" "$runtime_dir" \
        >"$output" 2>&1; then
        break
    fi

    if ! grep -q "noise-unsafe OpenFHE integer loop material" "$output"; then
        cat "$output" >&2
        exit 1
    fi

    if [[ "$attempt" == "5" ]]; then
        echo "build_mock runtime demo did not get noise-safe material after 5 attempts" >&2
        cat "$output" >&2
        exit 1
    fi
done

grep -q "Running v2 OpenFHE LWE integer runtime demo" "$output"
grep -q "OpenFHE LWE integer runtime boundary audit passed" "$output"
grep -q "GC_f(x', b') = GC{ \\[Dec_hsk(x') <= Dec_hsk(b')\\] }" "$output"
grep -q "Predicate sequence: 1,1,1,1,1,0" "$output"
grep -q "Encrypted loop iterations executed: 5" "$output"
