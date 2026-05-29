#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

out_dir="$(mktemp -d)"
trap 'rm -rf "$out_dir"' EXIT

cmake -S . -B build-local -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build build-local --target export_openfhe_lwe_int_material --parallel 2 >/dev/null

./build-local/export_openfhe_lwe_int_material \
    demo_keys/openfhe_binfhe_demo_keypair \
    "$out_dir/v2_lwe_integer_material.txt" >/dev/null

material="$out_dir/v2_lwe_integer_material.txt"

test -s "$material"

grep -q "OpenFHE LWE Integer Material Export" "$material"
grep -q "plaintext_modulus = 16" "$material"
grep -q "hsk.dimension = " "$material"
grep -q "hsk.s_raw = \\[" "$material"
grep -q "hsk.s_mod_q = \\[" "$material"
grep -q "a_prime.plaintext = 3" "$material"
grep -q "a_prime.manual_dec = 3" "$material"
grep -q "b_prime.plaintext = 7" "$material"
grep -q "b_prime.manual_dec = 7" "$material"
grep -q "one_prime.plaintext = 1" "$material"
grep -q "one_prime.manual_dec = 1" "$material"
