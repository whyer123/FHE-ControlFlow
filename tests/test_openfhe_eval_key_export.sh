#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

out_dir="$(mktemp -d)"
trap 'rm -rf "$out_dir"' EXIT
key_dir="$out_dir/openfhe_keypair"

cmake -S . -B build-local -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build build-local \
    --target generate_openfhe_keypair \
    --target export_openfhe_eval_key_material \
    --parallel 2 >/dev/null

./build-local/generate_openfhe_keypair "$key_dir" >/dev/null

./build-local/export_openfhe_eval_key_material \
    "$key_dir" \
    "$out_dir" >/dev/null

material="$out_dir/openfhe_fixed_eval_key_material.cpp"
manifest="$out_dir/openfhe_fixed_eval_key_material_manifest.txt"

test -s "$material"
test -s "$manifest"

grep -q "static const FixedRingGSWEvalKey FIXED_REFRESH_KEY" "$material"
grep -q "static const FixedLweSwitchingKey FIXED_SWITCH_KEY" "$material"
grep -q "refresh.dim0=" "$manifest"
grep -q "switch.dim0=" "$manifest"
