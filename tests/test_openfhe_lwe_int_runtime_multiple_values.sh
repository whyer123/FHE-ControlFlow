#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

tmp_dir="$(mktemp -d)"
trap 'rm -rf "$tmp_dir"' EXIT

cmake -S . -B build-local -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build build-local \
    --target export_openfhe_lwe_int_material \
    --target setup_openfhe_lwe_int_gc_material \
    --target audit_openfhe_lwe_int_runtime_boundary \
    --target openfhe_lwe_int_runtime_demo \
    --parallel 2 >/dev/null

run_case() {
    local lhs="$1"
    local rhs="$2"
    local expected_sequence="$3"
    local expected_iterations="$4"

    local material="$tmp_dir/material_${lhs}_${rhs}.txt"
    local runtime_dir="$tmp_dir/runtime_${lhs}_${rhs}"
    local output="$tmp_dir/runtime_${lhs}_${rhs}.out"

    ./build-local/export_openfhe_lwe_int_material \
        demo_keys/openfhe_binfhe_demo_keypair \
        "$material" \
        "$lhs" \
        "$rhs" \
        1 >/dev/null

    ./build-local/setup_openfhe_lwe_int_gc_material \
        "$material" \
        "$runtime_dir" >/dev/null

    ./build-local/audit_openfhe_lwe_int_runtime_boundary \
        "$material" \
        "$runtime_dir" >/dev/null

    ./build-local/openfhe_lwe_int_runtime_demo "$runtime_dir" >"$output"

    grep -q "Predicate sequence: $expected_sequence" "$output"
    grep -q "Encrypted loop iterations executed: $expected_iterations" "$output"
    grep -q "Evaluator runtime did not load hpk or hsk" "$output"
}

run_case 0 0 "1,0" 1
run_case 1 3 "1,1,1,0" 3
run_case 3 7 "1,1,1,1,1,0" 5
run_case 7 3 "0" 0
run_case 8 7 "0" 0
