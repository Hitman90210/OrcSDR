#!/usr/bin/env bash
# Host-only shared SD regressions. Logs stay in the invocation directory.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
out="$PWD"
stamp="$(date -u +%Y%m%dT%H%M%SZ)"
bin="$(mktemp -d "${TMPDIR:-/tmp}/orcsdr-sd-storage.XXXXXX")"
summary="$out/sd-storage-$stamp.log"
ui="$root/apps/orcsdr-tab5/ui"
tests="$root/apps/orcsdr-tab5/tests"
flags=(-std=c++17 -O1 -g -Wall -Wextra -fsanitize=address,undefined
       -fno-sanitize-recover=all -ffunction-sections -fdata-sections
       -Wl,--gc-sections -I"$ui")
printf 'Host-only SD regression; UTC=%s; binaries=%s\n' "$stamp" "$bin" | tee "$summary"
run() {
  local name="$1"; shift
  local build_log="$out/sd-storage-$name-build-$stamp.log"
  local run_log="$out/sd-storage-$name-run-$stamp.log"
  if ! g++ "${flags[@]}" "$@" -o "$bin/$name" >"$build_log" 2>&1; then
    printf '%s BUILD FAIL: %s\n' "$name" "$build_log" | tee -a "$summary"
    return 1
  fi
  if ! "$bin/$name" >"$run_log" 2>&1; then
    printf '%s RUN FAIL: %s\n' "$name" "$run_log" | tee -a "$summary"
    return 1
  fi
  printf '%s PASS; build=%s; run=%s\n' "$name" "$build_log" "$run_log" | tee -a "$summary"
}
run dma_buffer "$tests/sdmmc_dma_buffer_tests.cpp" -I"$tests/maps/ui_stubs"
run write_plan "$root/tests/sd_benchmark_plan_tests.cpp"
run lora_history "$root/tests/lora_history_tests.cpp" "$ui/lora_history.cpp" "$ui/lora_packet_log.cpp"
run sstv_archive -DORCSDR_SSTV_TESTING -I"$tests" "$tests/sstv_archive_test.cpp" \
  "$ui/sstv_archive.cpp" "$ui/sstv_receiver.cpp" "$ui/sstv_modes.cpp" "$ui/sstv_auto_detect.cpp" -pthread
python3 "$root/tools/test_ft8_log_storage.py" >"$out/sd-storage-ft8-log-run-$stamp.log" 2>&1
printf 'ft8_log PASS; run=%s/sd-storage-ft8-log-run-%s.log\n' "$out" "$stamp" | tee -a "$summary"
python3 "$root/tests/sd_space_query_test.py" >"$out/sd-storage-space-query-run-$stamp.log" 2>&1
printf 'space_query PASS; run=%s/sd-storage-space-query-run-%s.log\n' "$out" "$stamp" | tee -a "$summary"
printf 'SD_STORAGE_HOST_PASS (no device, DMA hardware or power-cut qualification)\n' | tee -a "$summary"
