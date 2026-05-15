#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

output_file="$(mktemp)"
material_dir="$(mktemp -d)"
trap 'rm -f "$output_file"; rm -rf "$material_dir"' EXIT

FIXED_GC_ARTIFACT_DIR="$material_dir" ./build_mock.sh --fixed-setup >/dev/null
FIXED_GC_ARTIFACT_DIR="$material_dir" ./build_mock.sh --fixed-runtime >"$output_file"

grep -q "Evaluator runtime input policy: fixed GC_f(x')" "$output_file"
grep -q "Evaluator runtime loaded only a', one', evaluation material, and fixed GC_f" "$output_file"
grep -q "Evaluator runtime did not load hpk or hsk" "$output_file"
grep -q "Encrypted loop iterations executed: 5" "$output_file"

predicate_sequence="$(
    grep "GC_f(x') revealed predicate \\[x <= fixed_b\\]" "$output_file" |
        sed 's/.*= //' |
        tr '\n' ',' |
        sed 's/,$//'
)"

if [[ "$predicate_sequence" != "1,1,1,1,1,0" ]]; then
    echo "unexpected predicate sequence: $predicate_sequence" >&2
    exit 1
fi

circuit_dump="$material_dir/fixed_bound_circuit_g_demo.txt"
test -f "$circuit_dump"
grep -q "fixed_b_0" "$circuit_dump"

if grep -qE "^[[:space:]]+w[0-9]+ b_[0-9]+$" "$circuit_dump"; then
    echo "fixed-bound circuit still exposes b_i as public inputs" >&2
    exit 1
fi
