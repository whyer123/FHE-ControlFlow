#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

test -x run_formal_demo.sh
grep -q "run_emp_showcase.sh" run_formal_demo.sh
grep -q "GC backend: EMP half-gates" run_formal_demo.sh
grep -q "artifacts/formal_emp_showcase" run_formal_demo.sh
grep -q "formal_emp_demo_output.txt" run_formal_demo.sh
grep -q "FORMAL_EMP_SHOWCASE_DIR" run_formal_demo.sh
grep -q "tee" run_formal_demo.sh

grep -q "run_formal_demo.sh" run_demo.sh

if grep -q "docker-compose up --build" run_demo.sh; then
    echo "run_demo.sh must not launch the old prototype entrypoint" >&2
    exit 1
fi
