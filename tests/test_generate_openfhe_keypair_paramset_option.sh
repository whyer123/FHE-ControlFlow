#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

source_file="tools/generate_openfhe_keypair.cpp"

grep -q "ParseParamSet" "$source_file"
grep -q "ParamSetName" "$source_file"
grep -q "STD128" "$source_file"
grep -q "\\[paramset\\]" "$source_file"

if grep -q "GenerateBinFHEContext(TOY)" "$source_file"; then
    echo "generate_openfhe_keypair still hardcodes TOY parameters" >&2
    exit 1
fi

if grep -q 'Parameter set: `TOY`' "$source_file"; then
    echo "generate_openfhe_keypair manifest still hardcodes TOY" >&2
    exit 1
fi
