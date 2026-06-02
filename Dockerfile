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
    python3 \
    python3-pip \
    sudo \
    wget \
    xxd \
    && update-ca-certificates \
    && pip3 install --no-cache-dir "cmake>=3.28,<4" \
    && cmake --version \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Fast development target for EMP half-gates GC backend and mock FHE.
FROM base AS gc-mock

WORKDIR /opt/emp-toolkit

RUN git clone --depth 1 https://github.com/emp-toolkit/emp-tool.git \
    && cmake -S emp-tool -B emp-tool/build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build emp-tool/build --target emp-tool --parallel $(nproc) \
    && cmake --install emp-tool/build \
    && ldconfig

ENV USE_EMP_GC=1

WORKDIR /app
COPY . .

RUN ./build_mock.sh --fixed-all

CMD ["./build_mock.sh", "--fixed-all"]

# Full OpenFHE target for testing with the real BinFHE dependency.
FROM base AS openfhe-runtime

WORKDIR /opt

RUN git clone https://github.com/openfheorg/openfhe-development.git \
    && cd openfhe-development \
    && git checkout v1.1.4 \
    && cmake -S . -B build \
        -DBUILD_SHARED=ON \
        -DBUILD_UNITTESTS=OFF \
        -DBUILD_EXAMPLES=OFF \
        -DBUILD_BENCHMARKS=OFF \
        -DBINARY_ONLY=OFF \
        -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --parallel 2 \
    && cmake --install build \
    && ldconfig

WORKDIR /app
COPY . .

RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --parallel $(nproc)

CMD ["/bin/bash"]

# Combined OpenFHE + EMP target for the v2 runtime path:
#   OpenFHE generates real LWE integer ciphertext/key material.
#   EMP half-gates garbles/evaluates GC{[Dec_hsk(x') <= Dec_hsk(b')]}.
FROM openfhe-runtime AS openfhe-emp-runtime

WORKDIR /opt/emp-toolkit

RUN git clone --depth 1 https://github.com/emp-toolkit/emp-tool.git \
    && cmake -S emp-tool -B emp-tool/build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build emp-tool/build --target emp-tool --parallel $(nproc) \
    && cmake --install emp-tool/build \
    && ldconfig

ENV USE_EMP_GC=1

WORKDIR /app

RUN cmake -S . -B build-emp -DCMAKE_BUILD_TYPE=Release -DUSE_EMP_GC=ON \
    && cmake --build build-emp \
        --target active_garbled_circuit_backend_test \
        --target generate_openfhe_keypair \
        --target export_openfhe_lwe_int_material \
        --target setup_openfhe_lwe_int_gc_material \
        --target audit_openfhe_lwe_int_runtime_boundary \
        --target openfhe_lwe_int_runtime_demo \
        --parallel $(nproc) \
    && ./build-emp/active_garbled_circuit_backend_test

CMD ["/bin/bash"]
