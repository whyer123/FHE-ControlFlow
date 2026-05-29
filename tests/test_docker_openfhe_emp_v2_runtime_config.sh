#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

grep -q "FROM openfhe-runtime AS openfhe-emp-runtime" Dockerfile
grep -q "ENV USE_EMP_GC=1" Dockerfile
grep -q "install.py --deps --tool" Dockerfile
grep -q -- "-DUSE_EMP_GC=ON" Dockerfile
grep -q "openfhe_emp_v2_runtime:" docker-compose.yml
grep -q "target: openfhe-emp-runtime" docker-compose.yml
grep -q "export_openfhe_lwe_int_material" docker-compose.yml
grep -q "setup_openfhe_lwe_int_gc_material" docker-compose.yml
grep -q "audit_openfhe_lwe_int_runtime_boundary" docker-compose.yml
grep -q "openfhe_lwe_int_runtime_demo" docker-compose.yml
grep -q "Predicate sequence: 1,1,1,1,1,0" docker-compose.yml
