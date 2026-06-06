#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

test -x run_emp_showcase.sh
grep -q "docker compose build openfhe_emp_v2_runtime" run_emp_showcase.sh
grep -q "docker compose run --rm openfhe_emp_v2_runtime" run_emp_showcase.sh
grep -q "GC backend: EMP half-gates" run_emp_showcase.sh
grep -q "FORMAL_EMP_SHOWCASE_DIR" run_emp_showcase.sh

if grep -q "build-local" run_emp_showcase.sh; then
    echo "EMP showcase script must not use local minimal-fallback build output" >&2
    exit 1
fi
