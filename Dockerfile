# Ubuntu 22.04 environment for the FHE + controlled-reveal GC prototype.
FROM ubuntu:22.04 AS base

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    ca-certificates \
    cmake \
    git \
    libomp-dev \
    libssl-dev \
    ninja-build \
    openssl \
    pkg-config \
    xxd \
    && update-ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Fast development target for the in-repo GC backend and mock FHE.
FROM base AS gc-mock

COPY . .

RUN g++ -std=c++17 -DMOCK_OPENFHE -I. \
    src/fhe/fhe_context.cpp \
    src/gates/fhe_gates.cpp \
    src/gc/boolean_circuit.cpp \
    src/gc/controlled_reveal_circuit.cpp \
    src/gc/minimal_garbled_circuit.cpp \
    src/algorithms/fhe_arithmetic.cpp \
    src/algorithms/fhe_cmp.cpp \
    examples/demo_loop.cpp \
    -o demo_mock

CMD ["./demo_mock"]

# Full OpenFHE target for testing with the real BinFHE dependency.
FROM base AS openfhe-runtime

WORKDIR /opt

RUN git clone https://github.com/openfheorg/openfhe-development.git \
    && cd openfhe-development \
    && git checkout v1.1.4 \
    && cmake -S . -B build -DBUILD_SHARED=ON -DBINARY_ONLY=OFF -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --parallel 2 \
    && cmake --install build \
    && ldconfig

WORKDIR /app
COPY . .

RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --parallel $(nproc)

CMD ["/bin/bash"]
