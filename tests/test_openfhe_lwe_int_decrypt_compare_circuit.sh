#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

cmake -S . -B build-local -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build build-local --target openfhe_lwe_int_decrypt_compare_circuit_test --parallel 2 >/dev/null

./build-local/openfhe_lwe_int_decrypt_compare_circuit_test
