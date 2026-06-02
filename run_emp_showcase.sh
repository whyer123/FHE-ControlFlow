#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

echo "==> Building OpenFHE + EMP runtime image"
docker compose build openfhe_emp_v2_runtime

echo
echo "==> Running EMP-backed evaluator runtime demo"
echo "Expected evidence:"
echo "  GC backend: EMP half-gates"
echo "  Evaluator runtime did not load hpk or hsk"
echo "  Predicate sequence: 1,1,1,1,1,0"
echo

docker compose run --rm openfhe_emp_v2_runtime
