#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

out_dir="$(mktemp -d)"
trap 'rm -rf "$out_dir"' EXIT

g++ -std=c++17 -DMOCK_OPENFHE -I. \
    src/gc/boolean_circuit.cpp \
    src/gc/boolean_circuit_export.cpp \
    src/gc/openfhe_lwe_decryption_circuit.cpp \
    src/gc/openfhe_eval_bin_gate_circuit.cpp \
    tools/dump_openfhe_eval_bin_gate_circuit.cpp \
    -o /tmp/dump_openfhe_eval_bin_gate_circuit

/tmp/dump_openfhe_eval_bin_gate_circuit "$out_dir" >"$out_dir/output.txt"

grep -q "Dumped OpenFHE EvalBinGate Boolean circuits" "$out_dir/output.txt"
grep -q "bootstrap core status: placeholder" "$out_dir/output.txt"

for gate in and or xor xnor; do
    dump="$out_dir/openfhe_evalbingate_${gate}_demo.txt"
    test -f "$dump"
    grep -q "OpenFHE.EvalBinGate" "$dump"
    grep -q "openfhe_evalbingate_prebootstrap" "$dump"
    grep -q "openfhe_bootstrap_placeholder" "$dump"
    grep -q "evalbingate_out_b_bit_0" "$dump"
done
