# Isolated mainline reconciliation — October 23, 2026

Parents: maps checkpoint b567047d (application cb1b567a) and fetched main
629ba546. Prepared in F:/AI/OrcSDR-TEMP/maps-mainline-reconcile-20261023 on
codex/maps-mainline-reconcile-20261023, preserving the original integration
checkout, all untracked logs/recovery artifacts and main/remote references.

The current acceptance matrix is [completion acceptance](2026-10-23-completion-acceptance.md).
This is a scoped reconciliation, not a whole-branch merge-readiness review.

## Resolution

Only eight paths overlap between the two lines of work. Merge-tree predicts
five conflict files; actual isolated --no-commit merge confirms them.

- ft8_runtime.cpp: both versions already place the same shared 4096-sample
  decoder chunk in PSRAM; retain one declaration, not duplicate buffers.
- main.cpp: retain both Maps and Dead Air exclusions from generic SDR repaint.
- screen_controller.cpp/.hpp: retain both surfaces. Preserve mainline's Dead Air
  ID 20 and append Maps as ID 21; named controls and enum-based ownership remain
  unchanged. Do not confuse this internal screen ID with map-screen mode values.
- tests/ft8_dashboard_tests.cpp: preserve both independent clock-sync checks and
  OrcMaps pending/ready/projection/label/allocation-fallback checks.

Auto-merged FT8 clock/Labels geometry and touch routing are inspected. Their
rectangles overlap numerically but belong to different tabs, so no production
layout change is warranted. Both normal and sanitized merged tests pass.

No pin, lock, RTL driver, C6 input, SD format or world data change is made by
conflict resolution. Newer mainline Dead Air and FT8 work is preserved as baseline,
not reimplemented or refactored. The source-resolution step used neither device. The subsequent native gate below uses COM17 only; COM39 remains untouched.

## Actual checks

All logs are beside invocation in the isolated candidate root:

- maps-mainline-ft8-dashboard-20261023.log: real FT8 dashboard test PASS.
- maps-mainline-ft8-dashboard-asan-20261023.log: same test ASan/UBSan PASS.
- maps-mainline-ft8-clock-20261023.log: clock estimator tests PASS.
- maps-mainline-clock-status-20261023.log: clock status tests PASS.
- maps-mainline-host-{configure,build,tests}-20261023.log: affected CTest cases
  adsb_map_range, lora_map_controls, map_view, map_pipeline, map_read_cache PASS 5/5.
- maps-mainline-p4-syntax-20261023.log: four units (FT8 dashboard/runtime,
  screen controller, main) compile with the real P4 compiler and saved SDK inputs.
  This is syntax only, not a candidate native configure/link or device pass.
- maps-mainline-diff-check-20261023.log: combined staged diff check exit 0.

Commands run from the candidate root under WSL Ubuntu:

```bash
cmake -S apps/orcsdr-tab5/tests/maps -B .local/maps/maps-host -DORCMAPS_ROOT=/mnt/f/AI/OrcSDR-TEMP/orcmaps-quality-delivery
cmake --build .local/maps/maps-host --target orcsdr_maps_tests orcsdr_map_view_tests orcsdr_lora_map_tests orcsdr_adsb_map_range_tests orcsdr_map_read_cache_tests -j2
ctest --test-dir .local/maps/maps-host -R "^(map_pipeline|map_view|lora_map_controls|adsb_map_range|map_read_cache)$" --output-on-failure
```

The targeted FT8 dashboard commands compile tests/ft8_dashboard_tests.cpp with
ft8_dashboard/model/conditions/hunter/tuning/info_text/focus_nav/clock_sync_status,
C++17, -Wall -Wextra -Werror -pedantic and tests/ft8_ui_stubs. Normal uses -O2;
sanitized uses -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer, with
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 and UBSAN_OPTIONS=halt_on_error=1.
The two clock test binaries compile their matching single source units at -O2.

## Native build and COM17 installation

Reconciled application source: 30a00ce1. Exact build command from the integration root:

```powershell
python .local/maps/build_pilot_20261022.py build maps-mainline-30a00ce1-20261023
```

Native build exit 0. Artifact `.local/maps/maps-mainline-30a00ce1-20261023/`:
application 5,257,280 bytes, partition headroom 1,034,176 bytes;
application SHA-256 `8edab674b2ebe3bf8d92e13fc562291ba4e8359bd0feaa77dce16c9c3de65506`.
Inspection passes unchanged 40 KiB DMA reserve, exception reserve, engine pin/lock,
FT8/miniz symbol gates and external-BSS placement. The FT8 previous-snapshot
buffer is 6,848 bytes in PSRAM, including mainline's added clock fields.

COM17 identity 30:ED:A0:E2:E6:95 verified. Previous cb1b567a app verified against
its recovery artifact; fresh non-app partition backups saved. Only the P4 app
was written. Boot/table, NVS, PHY, coredump and world preservation checks pass;
C6 remains 3.0.6. Startup reaches Ready with SD, Wi-Fi and receiver available.
No physical display acceptance or band-specific decoding is implied.

Logs saved beside invocation in the integration root:

- fit-order-native-maps-mainline-30a00ce1-20261023-20261020.log (historical wrapper naming)
- maps-mainline-inspection-20261023.log
- maps-mainline-preflash-device-20261023.log
- maps-mainline-{verify-previous,backup-*,verify-backups,app-write,preservation}-20261023.log
- apps/orcsdr-tab5/maps-mainline-boot-native-monitor-20261023.log

Preflight observed 47,173,101 ms uptime, streaming 94.5 MHz FM, manual gain 445,
zero reported drops/overruns, 15,658,004 bytes free PSRAM and largest DMA block
6,912 bytes. This is an opportunistic uptime observation, not a prescribed soak.
The bounded postflight navigation/continuity result is recorded separately below.

## Remaining gate

Native configure/link, artifact inspection, app preservation and startup pass.
Bounded COM17 navigation postflight exits 0: Local Maps ownership, LoRa pan,
ADS-B range and FT8 map/labels/live return pass, then 94.5 MHz FM/manual 445
restored. Exact command, timings, memory and limits are in the current matrix.
Final physical acceptance and repeated pressure checks remain required. Main/remote branches are not
updated here. A broad final review/full suite remains separately scoped under
AGENTS.md. No performance or physical-appearance acceptance is inferred from
these host results. The next source step can fast-forward the local integration
checkout to this reviewed candidate while retaining its prior commit for recovery;
that is distinct from the separately approved final merge into main.
