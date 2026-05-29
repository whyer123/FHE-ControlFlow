#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

if [[ "${RUN_STD128_OPENFHE_TEST:-0}" != "1" ]]; then
    echo "skipping STD128 OpenFHE runtime demo; set RUN_STD128_OPENFHE_TEST=1"
    exit 0
fi

work_dir="$(mktemp -d /private/tmp/openfhe_std128_runtime.XXXXXX)"
trap 'rm -rf "$work_dir"' EXIT

key_dir="$work_dir/keys"
material="$work_dir/material.txt"
runtime_dir="$work_dir/runtime"
runtime_output="$work_dir/runtime_output.txt"
setup_output="$work_dir/setup_output.txt"

cmake -S . -B build-local -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build build-local \
    --target generate_openfhe_keypair \
    --target export_openfhe_lwe_int_material \
    --target setup_openfhe_lwe_int_gc_material \
    --target audit_openfhe_lwe_int_runtime_boundary \
    --target openfhe_lwe_int_runtime_demo \
    --parallel 2 >/dev/null

./build-local/generate_openfhe_keypair "$key_dir" STD128 >/dev/null
grep -q "Parameter set: \`STD128\`" "$key_dir/manifest.md"

for attempt in 1 2 3 4 5; do
    rm -rf "$runtime_dir"
    ./build-local/export_openfhe_lwe_int_material "$key_dir" "$material" >/dev/null
    grep -q "plaintext_modulus = 16" "$material"
    grep -q "a_prime.dimension = 556" "$material"
    grep -q "a_prime.ciphertext_modulus = 2048" "$material"
    grep -q "a_prime.manual_dec = 3" "$material"
    grep -q "b_prime.manual_dec = 7" "$material"
    grep -q "one_prime.manual_dec = 1" "$material"

    if ./build-local/setup_openfhe_lwe_int_gc_material \
        "$material" "$runtime_dir" >"$setup_output" 2>&1; then
        break
    fi

    if ! grep -q "noise-unsafe OpenFHE integer loop material" "$setup_output"; then
        cat "$setup_output" >&2
        exit 1
    fi

    if [[ "$attempt" == "5" ]]; then
        echo "STD128 setup did not produce a noise-safe loop material after 5 attempts" >&2
        cat "$setup_output" >&2
        exit 1
    fi
done

grep -q "Runtime public input wires: 12254" "$setup_output"

./build-local/audit_openfhe_lwe_int_runtime_boundary \
    "$material" "$runtime_dir" >/dev/null

./build-local/openfhe_lwe_int_runtime_demo \
    "$runtime_dir" >"$runtime_output"
grep -q "Runtime public input wires: 12254" "$runtime_output"
grep -q "Predicate sequence: 1,1,1,1,1,0" "$runtime_output"
grep -q "Encrypted loop iterations executed: 5" "$runtime_output"
