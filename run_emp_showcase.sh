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

if [[ -n "${FORMAL_EMP_SHOWCASE_DIR:-}" ]]; then
    repo_root="$(pwd)"
    if [[ "$FORMAL_EMP_SHOWCASE_DIR" == "$repo_root/"* ]]; then
        container_artifact_dir="/app/${FORMAL_EMP_SHOWCASE_DIR#$repo_root/}"
    elif [[ "$FORMAL_EMP_SHOWCASE_DIR" == /* ]]; then
        echo "FORMAL_EMP_SHOWCASE_DIR must be relative or inside $repo_root" >&2
        exit 1
    else
        container_artifact_dir="/app/$FORMAL_EMP_SHOWCASE_DIR"
    fi

    docker compose run --rm \
        -e "FORMAL_EMP_SHOWCASE_DIR=$container_artifact_dir" \
        openfhe_emp_v2_runtime
else
    docker compose run --rm openfhe_emp_v2_runtime
fi
