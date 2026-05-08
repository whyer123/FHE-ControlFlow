#!/bin/bash
set -e

echo "Building Mock FHE Demo..."

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

# Compile everything with MOCK_OPENFHE defined.
g++ -std=c++17 ${EXTRA_CXXFLAGS} -DMOCK_OPENFHE ${EXTRA_DEFS} -I. \
    src/fhe/fhe_context.cpp \
    src/gates/fhe_gates.cpp \
    src/gc/boolean_circuit.cpp \
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
