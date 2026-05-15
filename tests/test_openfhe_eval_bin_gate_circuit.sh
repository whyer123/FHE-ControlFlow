#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

g++ -std=c++17 -DMOCK_OPENFHE -I. \
    src/gc/boolean_circuit.cpp \
    src/gc/boolean_circuit_export.cpp \
    src/gc/openfhe_lwe_decryption_circuit.cpp \
    src/gc/openfhe_eval_bin_gate_circuit.cpp \
    tests/openfhe_eval_bin_gate_circuit_test.cpp \
    -o /tmp/openfhe_eval_bin_gate_circuit_test

/tmp/openfhe_eval_bin_gate_circuit_test
