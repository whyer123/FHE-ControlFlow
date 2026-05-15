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
    sudo \
    wget \
    xxd \
    && update-ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Fast development target for EMP half-gates GC backend and mock FHE.
FROM base AS gc-mock

WORKDIR /opt/emp-toolkit

RUN wget https://raw.githubusercontent.com/emp-toolkit/emp-readme/master/scripts/install.py \
    && python3 install.py --deps --tool \
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
    && cmake -S . -B build -DBUILD_SHARED=ON -DBINARY_ONLY=OFF -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --parallel 2 \
    && cmake --install build \
    && ldconfig

WORKDIR /app
COPY . .

RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --parallel $(nproc)

CMD ["/bin/bash"]
