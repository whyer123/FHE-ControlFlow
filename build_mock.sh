#!/bin/bash
set -e

echo "Building Mock FHE Demo..."

# Compile everything with MOCK_OPENFHE defined.
g++ -std=c++17 -DMOCK_OPENFHE -I. \
    src/fhe/fhe_context.cpp \
    src/gates/fhe_gates.cpp \
    src/algorithms/fhe_cmp.cpp \
    src/selector/trusted_selector.cpp \
    src/pipeline/loop_controller.cpp \
    src/gc/gc_builder.cpp \
    examples/demo_loop.cpp \
    -o demo_mock

echo "Build successful! Running demo_mock..."
echo "----------------------------------------"
./demo_mock
