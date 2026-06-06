#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

out_dir="${FORMAL_EMP_SHOWCASE_DIR:-artifacts/formal_emp_showcase}"
mkdir -p "$out_dir"
log_path="$out_dir/formal_emp_demo_output.txt"

{
    echo "==> Formal demo: OpenFHE STD128 + EMP half-gates runtime"
    echo "Artifacts will be saved to: $out_dir"
    echo "Expected evidence:"
    echo "  GC backend: EMP half-gates"
    echo "  Evaluator runtime did not load hpk or hsk"
    echo "  Predicate sequence: 1,1,1,1,1,0"
    echo

    FORMAL_EMP_SHOWCASE_DIR="$out_dir" ./run_emp_showcase.sh

    echo
    echo "Saved formal EMP demo output to:"
    echo "  $log_path"
    echo "Saved formal EMP demo artifacts to:"
    echo "  $out_dir"
} 2>&1 | tee "$log_path"
