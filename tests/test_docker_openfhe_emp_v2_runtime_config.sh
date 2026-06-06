#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

grep -q "FROM openfhe-runtime AS openfhe-emp-runtime" Dockerfile
grep -q -- "-DBUILD_UNITTESTS=OFF" Dockerfile
grep -q -- "-DBUILD_EXAMPLES=OFF" Dockerfile
grep -q -- "-DBUILD_BENCHMARKS=OFF" Dockerfile
grep -q "python3-pip" Dockerfile
grep -q "pip3 install --no-cache-dir" Dockerfile
grep -q "ENV USE_EMP_GC=1" Dockerfile
grep -q "git clone --depth 1 https://github.com/emp-toolkit/emp-tool.git" Dockerfile
grep -q -- "--target emp-tool" Dockerfile
grep -q "cmake --install" Dockerfile
grep -q -- "-DUSE_EMP_GC=ON" Dockerfile
grep -q "generate_openfhe_keypair" Dockerfile
grep -q "active_garbled_circuit_backend_test" Dockerfile
grep -q "openfhe_emp_v2_runtime:" docker-compose.yml
grep -q "target: openfhe-emp-runtime" docker-compose.yml
grep -q "cmake -S . -B /tmp/build-emp" docker-compose.yml
grep -q "generate_openfhe_keypair" docker-compose.yml
grep -q "STD128" docker-compose.yml
grep -q "export_openfhe_lwe_int_material" docker-compose.yml
grep -q "setup_openfhe_lwe_int_gc_material" docker-compose.yml
grep -q "audit_openfhe_lwe_int_runtime_boundary" docker-compose.yml
grep -q "openfhe_lwe_int_runtime_demo" docker-compose.yml
grep -q "for attempt in 1 2 3 4 5" docker-compose.yml
grep -q "noise-unsafe OpenFHE integer loop material" docker-compose.yml
grep -q "GC backend: EMP half-gates" docker-compose.yml
grep -q "FORMAL_EMP_SHOWCASE_DIR" docker-compose.yml
grep -q "openfhe_lwe_int_gc_artifact.bin" docker-compose.yml
grep -q "Predicate sequence: 1,1,1,1,1,0" docker-compose.yml
grep -Eq "^build-local/?$" .dockerignore
grep -Eq "^build-emp/?$" .dockerignore
grep -Eq "^artifacts/?$" .dockerignore
grep -Eq "^poster/?$" .dockerignore
grep -Eq "^demo_keys/?$" .dockerignore

if grep -q "demo_keys/openfhe_binfhe_demo_keypair" docker-compose.yml; then
    echo "openfhe_emp_v2_runtime must generate temporary keys, not use repo-local demo_keys" >&2
    exit 1
fi
