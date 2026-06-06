#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

counts="${LOOP_SCALE_COUNTS:-10 100 1000}"
out_root="${1:-artifacts/emp_loop_scale}"
out_dir="$out_root/latest"
image_name="${EMP_RUNTIME_IMAGE:-fhe-codex-openfhe_emp_v2_runtime}"

mkdir -p "$out_root"
rm -rf "$out_dir"
mkdir -p "$out_dir"

echo "==> Building EMP/OpenFHE runtime image"
docker compose build openfhe_emp_v2_runtime

echo "==> Running loop-scale experiment"
echo "Counts: $counts"
echo "Output: $out_dir"

docker run --rm \
    -e "LOOP_SCALE_COUNTS=$counts" \
    -e "OPENFHE_PARAMSET=${OPENFHE_PARAMSET:-STD128}" \
    -v "$PWD/$out_dir:/out" \
    "$image_name" \
    bash -lc '
set -euo pipefail

read -r -a counts <<< "${LOOP_SCALE_COUNTS}"
paramset="${OPENFHE_PARAMSET:-STD128}"
work_dir="/tmp/openfhe_loop_scale"
rm -rf "$work_dir"
mkdir -p "$work_dir"

echo "count	plaintext_modulus	setup_seconds	runtime_seconds	status" \
    > /out/summary.tsv

echo "Generating one OpenFHE keypair with paramset ${paramset}..."
/app/build-emp/generate_openfhe_keypair "$work_dir/keys" "$paramset" \
    > /out/keygen.log 2>&1

for count in "${counts[@]}"; do
    if [[ ! "$count" =~ ^[0-9]+$ ]] || [[ "$count" == "0" ]]; then
        echo "invalid count: $count" >&2
        exit 1
    fi

    p=16
    while (( p <= count )); do
        p=$((p * 2))
    done

    a=0
    b=$((count - 1))
    material="/out/full_setup_material_${count}.txt"
    runtime_dir="/out/runtime_${count}"
    export_log="/out/export_${count}.log"
    setup_log="/out/setup_${count}.log"
    audit_log="/out/audit_${count}.log"
    runtime_log="/out/runtime_${count}.log"

    echo
    echo "==> count=${count}, a=${a}, b=${b}, plaintext_modulus=${p}"

    setup_start="$(date +%s)"
    setup_ok=0
    status="setup_failed"

    for attempt in 1 2 3 4 5; do
        rm -rf "$runtime_dir"

        if ! /app/build-emp/export_openfhe_lwe_int_material \
            "$work_dir/keys" "$material" "$a" "$b" 1 "$p" \
            > "$export_log" 2>&1; then
            status="export_failed"
            break
        fi

        if /app/build-emp/setup_openfhe_lwe_int_gc_material \
            "$material" "$runtime_dir" > "$setup_log" 2>&1; then
            setup_ok=1
            status="setup_ok"
            break
        fi

        if ! grep -q "noise-unsafe OpenFHE integer loop material" "$setup_log"; then
            status="setup_failed"
            break
        fi
    done

    setup_seconds="$(( $(date +%s) - setup_start ))"

    if [[ "$setup_ok" != "1" ]]; then
        echo -e "${count}\t${p}\t${setup_seconds}\tNA\t${status}" \
            >> /out/summary.tsv
        echo "Stopping at count=${count}; see ${export_log} and ${setup_log}."
        break
    fi

    /app/build-emp/audit_openfhe_lwe_int_runtime_boundary \
        "$material" "$runtime_dir" > "$audit_log" 2>&1

    runtime_start="$(date +%s)"
    /app/build-emp/openfhe_lwe_int_runtime_demo "$runtime_dir" \
        > "$runtime_log" 2>&1
    runtime_seconds="$(( $(date +%s) - runtime_start ))"

    if ! grep -q "Encrypted loop iterations executed: ${count}" "$runtime_log"; then
        status="runtime_unexpected"
        echo -e "${count}\t${p}\t${setup_seconds}\t${runtime_seconds}\t${status}" \
            >> /out/summary.tsv
        echo "Stopping at count=${count}; runtime did not report expected iterations."
        break
    fi

    status="ok"
    echo -e "${count}\t${p}\t${setup_seconds}\t${runtime_seconds}\t${status}" \
        >> /out/summary.tsv
    grep -E "GC backend|Predicate sequence|Encrypted loop iterations executed" \
        "$runtime_log" || true
done

echo
echo "Summary:"
cat /out/summary.tsv
'

echo
echo "Saved loop-scale experiment files to:"
echo "  $out_dir"
