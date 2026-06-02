#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

tmp_dir="$(mktemp -d)"
trap 'rm -rf "$tmp_dir"' EXIT

key_dir="$tmp_dir/openfhe_keypair"
full_material="$tmp_dir/full_v2_lwe_integer_material.txt"
runtime_dir="$tmp_dir/runtime"
runtime_output="$tmp_dir/runtime_output.txt"
setup_output="$tmp_dir/setup_output.txt"

cmake -S . -B build-local -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build build-local \
    --target generate_openfhe_keypair \
    --target export_openfhe_lwe_int_material \
    --target setup_openfhe_lwe_int_gc_material \
    --target audit_openfhe_lwe_int_runtime_boundary \
    --target openfhe_lwe_int_runtime_demo \
    --parallel 2 >/dev/null

./build-local/generate_openfhe_keypair "$key_dir" >/dev/null

for attempt in 1 2 3 4 5; do
    rm -rf "$runtime_dir"
    ./build-local/export_openfhe_lwe_int_material \
        "$key_dir" \
        "$full_material" >/dev/null

    if ./build-local/setup_openfhe_lwe_int_gc_material \
        "$full_material" \
        "$runtime_dir" >"$setup_output" 2>&1; then
        break
    fi

    if ! grep -q "noise-unsafe OpenFHE integer loop material" "$setup_output"; then
        cat "$setup_output" >&2
        exit 1
    fi

    if [[ "$attempt" == "5" ]]; then
        echo "setup did not produce noise-safe loop material after 5 attempts" >&2
        cat "$setup_output" >&2
        exit 1
    fi
done

./build-local/audit_openfhe_lwe_int_runtime_boundary \
    "$full_material" \
    "$runtime_dir" >/dev/null

test -f "$runtime_dir/openfhe_lwe_int_runtime_material.txt"
test -f "$runtime_dir/openfhe_lwe_int_circuit_shape.bin"
test -f "$runtime_dir/openfhe_lwe_int_gc_artifact.bin"

if grep -Eq '(^|[.])hsk[.=]' "$runtime_dir/openfhe_lwe_int_runtime_material.txt"; then
    echo "runtime material must not contain hsk fields" >&2
    exit 1
fi

if grep -Eq '(^|[.])(plaintext|manual_dec)=' "$runtime_dir/openfhe_lwe_int_runtime_material.txt"; then
    echo "runtime material must not contain clear plaintext/decryption fields" >&2
    exit 1
fi

./build-local/openfhe_lwe_int_runtime_demo "$runtime_dir" >"$runtime_output"

grep -q "Evaluator runtime did not load hpk or hsk" "$runtime_output"
grep -q "GC backend: in-repo minimal fallback" "$runtime_output"
grep -q "GC_f(x', b') = GC{ \\[Dec_hsk(x') <= Dec_hsk(b')\\] }" "$runtime_output"
grep -q "Predicate sequence: 1,1,1,1,1,0" "$runtime_output"
grep -q "Encrypted loop iterations executed: 5" "$runtime_output"
