#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

out_dir="$(mktemp -d)"
trap 'rm -rf "$out_dir"' EXIT

cmake -S . -B build-local -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build build-local --target generate_openfhe_keypair --parallel 2 >/dev/null

if ./build-local/generate_openfhe_keypair \
    "$out_dir/keys" SHOULD_FAIL >"$out_dir/stdout.txt" 2>"$out_dir/stderr.txt"; then
    echo "generate_openfhe_keypair accepted an unknown paramset" >&2
    exit 1
fi

grep -q "unsupported BinFHE paramset 'SHOULD_FAIL'" "$out_dir/stderr.txt"

if grep -q "libc++abi\\|uncaught exception\\|terminating" "$out_dir/stderr.txt"; then
    echo "generate_openfhe_keypair reported an uncaught exception" >&2
    cat "$out_dir/stderr.txt" >&2
    exit 1
fi

if [[ -e "$out_dir/keys" ]]; then
    echo "generate_openfhe_keypair created output for an invalid paramset" >&2
    exit 1
fi
