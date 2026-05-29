#!/bin/bash
set -e

echo "Building Mock FHE Demo..."
MODE="${1:---legacy}"
FIXED_GC_ARTIFACT_DIR="${FIXED_GC_ARTIFACT_DIR:-artifacts}"

EXTRA_DEFS=""
EXTRA_SRCS=""
EXTRA_LIBS=""
EXTRA_CXXFLAGS=""
if [[ "${USE_EMP_GC:-0}" == "1" ]]; then
    echo "EMP half-gates GC backend enabled."
    EXTRA_DEFS="-DUSE_EMP_GC"
    EXTRA_SRCS="src/gc/emp_garbled_circuit.cpp"
    EXTRA_LIBS="-lemp-tool -lssl -lcrypto -pthread"
    case "$(uname -m)" in
        aarch64|arm64)
            EXTRA_CXXFLAGS="-march=armv8-a+simd+crypto+crc"
            ;;
        x86_64|amd64)
            EXTRA_CXXFLAGS="-maes -msse4.1"
            ;;
    esac
else
    echo "Using in-repo minimal GC backend."
fi

build_legacy_demo() {
    g++ -std=c++17 ${EXTRA_CXXFLAGS} -DMOCK_OPENFHE ${EXTRA_DEFS} -I. \
        src/fhe/fhe_context.cpp \
        src/gates/fhe_gates.cpp \
        src/gc/boolean_circuit.cpp \
        src/gc/boolean_circuit_export.cpp \
        src/gc/controlled_reveal_circuit.cpp \
        src/gc/garbled_predicate_evaluator.cpp \
        src/gc/minimal_garbled_circuit.cpp \
        src/gc/openfhe_lwe_decryption_circuit.cpp \
        ${EXTRA_SRCS} \
        src/algorithms/fhe_arithmetic.cpp \
        src/algorithms/fhe_cmp.cpp \
        examples/demo_loop.cpp \
        ${EXTRA_LIBS} \
        -o demo_mock

    echo "Build successful! Running demo_mock..."
    echo "----------------------------------------"
    ./demo_mock
}

build_fixed_setup() {
    g++ -std=c++17 ${EXTRA_CXXFLAGS} -DMOCK_OPENFHE ${EXTRA_DEFS} -I. \
        src/fhe/fhe_context.cpp \
        src/gates/fhe_gates.cpp \
        src/gc/active_garbled_circuit_io.cpp \
        src/gc/boolean_circuit.cpp \
        src/gc/boolean_circuit_export.cpp \
        src/gc/boolean_circuit_io.cpp \
        src/gc/controlled_reveal_circuit.cpp \
        src/gc/garbled_predicate_evaluator.cpp \
        src/gc/minimal_garbled_circuit.cpp \
        src/gc/openfhe_lwe_decryption_circuit.cpp \
        ${EXTRA_SRCS} \
        tools/setup_fixed_gc_material.cpp \
        ${EXTRA_LIBS} \
        -o setup_fixed_gc_material

    echo "Build successful! Preparing fixed GC material..."
    echo "----------------------------------------"
    ./setup_fixed_gc_material "${FIXED_GC_ARTIFACT_DIR}"
}

build_fixed_runtime() {
    g++ -std=c++17 ${EXTRA_CXXFLAGS} -DMOCK_OPENFHE ${EXTRA_DEFS} -I. \
        src/gc/active_garbled_circuit_io.cpp \
        src/gc/boolean_circuit.cpp \
        src/gc/boolean_circuit_io.cpp \
        src/gc/garbled_predicate_evaluator.cpp \
        src/gc/minimal_garbled_circuit.cpp \
        ${EXTRA_SRCS} \
        examples/fixed_runtime_demo.cpp \
        ${EXTRA_LIBS} \
        -o fixed_runtime_demo

    echo "Build successful! Running fixed_runtime_demo..."
    echo "----------------------------------------"
    ./fixed_runtime_demo "${FIXED_GC_ARTIFACT_DIR}"
}

build_evalbingate_circuit_dump() {
    g++ -std=c++17 -DMOCK_OPENFHE -I. \
        src/gc/boolean_circuit.cpp \
        src/gc/boolean_circuit_export.cpp \
        src/gc/openfhe_lwe_decryption_circuit.cpp \
        src/gc/openfhe_eval_bin_gate_circuit.cpp \
        tools/dump_openfhe_eval_bin_gate_circuit.cpp \
        -o dump_openfhe_eval_bin_gate_circuit

    echo "Build successful! Dumping OpenFHE EvalBinGate Boolean circuits..."
    echo "----------------------------------------"
    ./dump_openfhe_eval_bin_gate_circuit "${FIXED_GC_ARTIFACT_DIR}"
}

build_v2_openfhe_gc_test() {
    g++ -std=c++17 ${EXTRA_CXXFLAGS} ${EXTRA_DEFS} -I. \
        src/gc/boolean_circuit.cpp \
        src/gc/minimal_garbled_circuit.cpp \
        src/gc/openfhe_lwe_int_decrypt_compare_circuit.cpp \
        ${EXTRA_SRCS} \
        tests/openfhe_lwe_int_decrypt_compare_gc_test.cpp \
        ${EXTRA_LIBS} \
        -o openfhe_lwe_int_decrypt_compare_gc_test

    echo "Build successful! Running v2 OpenFHE decrypt-compare GC test..."
    echo "----------------------------------------"
    ./openfhe_lwe_int_decrypt_compare_gc_test
}

case "${MODE}" in
    --legacy)
        build_legacy_demo
        ;;
    --fixed-setup)
        build_fixed_setup
        ;;
    --fixed-runtime)
        build_fixed_runtime
        ;;
    --fixed-all)
        build_fixed_setup
        build_fixed_runtime
        ;;
    --evalbingate-circuit)
        build_evalbingate_circuit_dump
        ;;
    --v2-openfhe-gc-test)
        build_v2_openfhe_gc_test
        ;;
    *)
        echo "unknown build_mock.sh mode: ${MODE}" >&2
        exit 1
        ;;
esac
