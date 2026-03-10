# Start from Ubuntu 22.04 and install dependencies for OpenFHE
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    libomp-dev \
    libssl-dev \
    xxd \
    pkg-config \
    && rm -rf /var/lib/apt/lists/*

# Install OpenFHE
WORKDIR /opt
RUN git clone https://github.com/openfheorg/openfhe-development.git \
    && cd openfhe-development \
    && git checkout v1.1.4 \
    && mkdir build && cd build \
    && cmake .. -DBUILD_SHARED=ON -DBINARY_ONLY=OFF \
    && make -j2 \
    && make install \
    && ldconfig

# Set up project directory
WORKDIR /app
COPY . .

# Build the project
RUN mkdir -p build && cd build \
    && cmake .. \
    && make -j$(nproc)

CMD ["/bin/bash"]
