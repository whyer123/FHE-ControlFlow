#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

tmp_dir="$(mktemp -d)"
trap 'rm -rf "$tmp_dir"' EXIT
key_dir="$tmp_dir/openfhe_keypair"

cmake -S . -B build-local -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build build-local \
    --target generate_openfhe_keypair \
    --target export_openfhe_lwe_int_material \
    --target openfhe_lwe_int_decrypt_compare_circuit_test \
    --target openfhe_lwe_int_decrypt_compare_gc_test \
    --parallel 2 >/dev/null

./build-local/generate_openfhe_keypair "$key_dir" >/dev/null

for pair in "0 0" "0 15" "7 3" "8 7" "15 15"; do
    read -r lhs rhs <<<"$pair"
    material="$tmp_dir/material_${lhs}_${rhs}.txt"
    ./build-local/export_openfhe_lwe_int_material \
        "$key_dir" \
        "$material" \
        "$lhs" \
        "$rhs" \
        1 >/dev/null

    grep -q "a_prime.plaintext = $lhs" "$material"
    grep -q "b_prime.plaintext = $rhs" "$material"
    ./build-local/openfhe_lwe_int_decrypt_compare_circuit_test "$material"
    ./build-local/openfhe_lwe_int_decrypt_compare_gc_test "$material"
done
