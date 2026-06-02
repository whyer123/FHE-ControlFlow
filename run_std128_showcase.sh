#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

# 這個腳本會跑完整 STD128 展示流程：
#   1. client/setup 階段：產生 OpenFHE key、加密 a/b/one、
#      建立 Boolean circuit，並把它 garble 成 GC artifact。
#   2. evaluator/runtime 階段：只載入 runtime material + GC artifact，
#      在沒有 hpk/hsk 的情況下執行加密 loop。
#
# 產生的資料夾包含 setup secret material，不要 commit。
out_dir="${1:-artifacts/std128_showcase}"
max_attempts="${STD128_SHOWCASE_MAX_ATTEMPTS:-5}"

# 只屬於 client/setup 的輸出。這些檔案包含 hpk/hsk 或 hsk 衍生資料。
key_dir="$out_dir/keys"
material="$out_dir/full_setup_material.txt"

# evaluator 可以看到的 runtime 目錄。setup 完成後，evaluator runtime
# 只需要這個目錄。
runtime_dir="$out_dir/runtime"

# 保留下來給展示與除錯用的 log。
setup_log="$out_dir/setup.log"
runtime_log="$out_dir/runtime_output.txt"
showcase_log="$out_dir/showcase.log"

if [[ -e "$out_dir" ]]; then
    echo "Output directory already exists: $out_dir" >&2
    echo "Delete it first if you want a fresh STD128 showcase:" >&2
    echo "  rm -rf $out_dir" >&2
    exit 1
fi

mkdir -p "$out_dir"

# 把腳本全部輸出同步寫到 showcase.log，方便展示後稽核整個流程。
exec > >(tee "$showcase_log") 2>&1

total_start="$(date +%s)"

step() {
    echo
    echo "==> $*"
}

elapsed() {
    local start="$1"
    local end
    end="$(date +%s)"
    echo "$((end - start))s"
}

# 編譯展示會用到的執行檔。這些 target 定義在 CMakeLists.txt：
#   generate_openfhe_keypair              -> 產生 STD128 hpk/hsk
#   export_openfhe_lwe_int_material       -> 產生 a'、b'、one' 和 hsk.s_mod_q
#   setup_openfhe_lwe_int_gc_material     -> 建立 circuit + GC artifact
#   audit_openfhe_lwe_int_runtime_boundary -> 檢查 runtime 不含 hpk/hsk
#   openfhe_lwe_int_runtime_demo          -> evaluator-only loop runtime
step "Building required OpenFHE/GC targets"
stage_start="$(date +%s)"
cmake -S . -B build-local -DCMAKE_BUILD_TYPE=Release
cmake --build build-local \
    --target generate_openfhe_keypair \
    --target export_openfhe_lwe_int_material \
    --target setup_openfhe_lwe_int_gc_material \
    --target audit_openfhe_lwe_int_runtime_boundary \
    --target openfhe_lwe_int_runtime_demo \
    --parallel 2
echo "Build time: $(elapsed "$stage_start")"

# client/setup 階段：產生一組新的 STD128 OpenFHE keypair。
# 如果刪掉 out_dir 後重跑這個腳本，會產生不同的 hsk，
# 因此也會產生不同的 GC artifact。
step "Generating STD128 OpenFHE keypair"
stage_start="$(date +%s)"
./build-local/generate_openfhe_keypair "$key_dir" STD128
echo "Key generation time: $(elapsed "$stage_start")"

setup_ok=0
for attempt in $(seq 1 "$max_attempts"); do
    # client/setup 階段：
    #   export_openfhe_lwe_int_material 會用 hpk 加密：
    #     a' = Enc(3), b' = Enc(7), one' = Enc(1)
    #   它也會匯出 hsk.s_mod_q，供 GC setup 使用。
    #
    #   setup_openfhe_lwe_int_gc_material 會建立：
    #     GC{ [Dec_hsk(x') <= Dec_hsk(b')] }
    #   並且只把清理過的 runtime material 寫到 runtime_dir。
    step "Exporting material and building GC artifact, attempt $attempt/$max_attempts"
    stage_start="$(date +%s)"
    rm -rf "$runtime_dir"
    ./build-local/export_openfhe_lwe_int_material "$key_dir" "$material"

    if ./build-local/setup_openfhe_lwe_int_gc_material \
        "$material" "$runtime_dir" >"$setup_log" 2>&1; then
        cat "$setup_log"
        setup_ok=1
        echo "Setup/GC time: $(elapsed "$stage_start")"
        break
    fi

    cat "$setup_log"

    # 使用真實 LWE ciphertext 時，one' 單獨解密可能正確等於 1，
    # 但連續加很多次後 noise 可能讓結果跳掉。遇到這種情況，
    # setup 會拒絕這份 material，並用同一組 key 重新加密新的 a'/b'/one'。
    if grep -q "noise-unsafe OpenFHE integer loop material" "$setup_log"; then
        echo "Material was noise-unsafe for this loop; retrying with fresh ciphertexts."
        continue
    fi

    echo "Setup failed for a non-retryable reason." >&2
    exit 1
done

if [[ "$setup_ok" != "1" ]]; then
    echo "Could not produce a noise-safe STD128 showcase after $max_attempts attempts." >&2
    exit 1
fi

# boundary audit 用來證明 evaluator runtime 目錄不包含：
#   hpk、hsk、hsk.s_mod_q、manual_dec 或明文 plaintext 欄位。
step "Auditing runtime boundary"
./build-local/audit_openfhe_lwe_int_runtime_boundary "$material" "$runtime_dir"

# evaluator/runtime 階段：
#   openfhe_lwe_int_runtime_demo 只接收 runtime_dir。
#   它會載入 a'、b'、one'、circuit shape 和 GC artifact。
#   它不會載入 hpk、hsk 或 full_setup_material。
step "Running evaluator runtime demo"
stage_start="$(date +%s)"
./build-local/openfhe_lwe_int_runtime_demo "$runtime_dir" | tee "$runtime_log"
echo "Runtime demo time: $(elapsed "$stage_start")"

# 印出檔案大小，方便展示：
#   - GC 前的 Boolean circuit
#   - garbled GC artifact
#   - evaluator runtime material
#   - setup-only secret material
step "Showcase files"
find "$out_dir" -maxdepth 2 -type f -exec ls -lh {} +

echo
echo "Boolean circuit before GC:"
echo "  $runtime_dir/openfhe_lwe_int_circuit_g_demo.txt"
echo
echo "Garbled circuit artifact:"
echo "  $runtime_dir/openfhe_lwe_int_gc_artifact.bin"
echo
echo "Evaluator runtime material:"
echo "  $runtime_dir/openfhe_lwe_int_runtime_material.txt"
echo
echo "Setup-only secret material, do not give to evaluator:"
echo "  $material"
echo "  $key_dir"
echo
echo "Total showcase time: $(elapsed "$total_start")"
echo
echo "After the demo, delete generated STD128 material with:"
echo "  rm -rf $out_dir"
