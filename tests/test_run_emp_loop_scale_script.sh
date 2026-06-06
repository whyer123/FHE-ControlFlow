#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

test -x run_emp_loop_scale.sh
grep -q "LOOP_SCALE_COUNTS:-10 100 1000" run_emp_loop_scale.sh
grep -q "artifacts/emp_loop_scale" run_emp_loop_scale.sh
grep -q "docker compose build openfhe_emp_v2_runtime" run_emp_loop_scale.sh
grep -q "docker run --rm" run_emp_loop_scale.sh
grep -q "export_openfhe_lwe_int_material" run_emp_loop_scale.sh
grep -q '\"\$a\" \"\$b\" 1 \"\$p\"' run_emp_loop_scale.sh
grep -q "Encrypted loop iterations executed: \${count}" run_emp_loop_scale.sh
grep -q "summary.tsv" run_emp_loop_scale.sh
grep -Eq "^artifacts/?$" .gitignore
