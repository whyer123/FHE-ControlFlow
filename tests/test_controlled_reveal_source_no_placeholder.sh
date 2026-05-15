#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

source_file="src/gc/openfhe_controlled_reveal_reference.cpp"

if grep -q "DecodeOpenFHEGateInputPhase" "$source_file"; then
    echo "controlled reveal source still contains semantic gate decoding" >&2
    exit 1
fi

if grep -q "EncryptBitWithFixedMask" "$source_file"; then
    echo "controlled reveal source still re-encrypts semantic gate bits" >&2
    exit 1
fi

grep -q "BootstrapGateCoreOpenFHE" "$source_file"
grep -q "EvalAccCGGI" "$source_file"
grep -q "SwitchCTtoqn" "$source_file"
