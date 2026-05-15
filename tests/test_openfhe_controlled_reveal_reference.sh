#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

cmake -S . -B build-local -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build build-local --target openfhe_controlled_reveal_reference --parallel 2 >/dev/null

output_file="$(mktemp)"
trap 'rm -f "$output_file"' EXIT

./build-local/openfhe_controlled_reveal_reference \
    demo_keys/openfhe_binfhe_demo_keypair >"$output_file"

grep -Fq "Reference expression: Dec_hsk(OpenFHE.Eval([x <= b], x', b'))" "$output_file"
grep -Fq "OpenFHE EvalBinGate is inside the predicate computation." "$output_file"
grep -Fq "hpk was not loaded by this reference program." "$output_file"
grep -Fq "hsk was loaded only to perform the final controlled reveal." "$output_file"
grep -Fq "Predicate sequence: 1,1,1,1,1,0" "$output_file"
grep -Fq "Encrypted loop iterations executed: 5" "$output_file"
