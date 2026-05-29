#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

tmp_dir="$(mktemp -d)"
trap 'rm -rf "$tmp_dir"' EXIT

material="$tmp_dir/noisy_increment_material.txt"
runtime_dir="$tmp_dir/runtime"
setup_output="$tmp_dir/setup_output.txt"

cat >"$material" <<'MATERIAL'
format = openfhe_lwe_int_setup_material_v1
logical_plaintext_bits = 4
plaintext_modulus = 16
hsk.dimension = 1
hsk.modulus = 512
hsk.switched_modulus = 512
hsk.s_mod_q = [0]

a_prime.plaintext = 3
a_prime.dimension = 1
a_prime.ciphertext_modulus = 512
a_prime.plaintext_modulus = 16
a_prime.body = 96
a_prime.a = [0]
a_prime.manual_dec = 3

b_prime.plaintext = 7
b_prime.dimension = 1
b_prime.ciphertext_modulus = 512
b_prime.plaintext_modulus = 16
b_prime.body = 224
b_prime.a = [0]
b_prime.manual_dec = 7

one_prime.plaintext = 1
one_prime.dimension = 1
one_prime.ciphertext_modulus = 512
one_prime.plaintext_modulus = 16
one_prime.body = 47
one_prime.a = [0]
one_prime.manual_dec = 1
MATERIAL

cmake -S . -B build-local -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build build-local \
    --target setup_openfhe_lwe_int_gc_material \
    --parallel 2 >/dev/null

if ./build-local/setup_openfhe_lwe_int_gc_material \
    "$material" \
    "$runtime_dir" >"$setup_output" 2>&1; then
    echo "setup accepted material whose one' increment skips loop states" >&2
    exit 1
fi

grep -q "noise-unsafe OpenFHE integer loop material" "$setup_output"
