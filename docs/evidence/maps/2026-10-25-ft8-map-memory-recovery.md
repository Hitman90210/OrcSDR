# FT8 map memory recovery

COM17 lost the FT8 basemap after the V3c LoRa check. Read-only serial evidence in
`ft8-missing-map-readonly-device-20261025.log` reported 602,692 free PSRAM bytes,
largest block 524,288 bytes, and 15,840,000 bytes retained by LoRa IQ buffers.
FT8 was decoding. The user restarted and confirmed the map returned. This
supports memory pressure as the cause, without proving every possible failure.
An FT8 SD log-write error was also captured and remains a separate investigation.

The FT8 map now calls the existing reclaim registry before allocating missing
sprites. Holder callbacks retain ownership of their safety checks: live captures,
exports and active decoding must not be forcibly freed. Allocation or map-worker
out-of-memory results schedule a retry every two seconds even if no worker
revision changes. A completed composed map remains visible during recovery.
Healthy ordinary updates do not reclaim again. Exit resets retry state.

No new framebuffer or DMA reserve is added. Existing FT8 sprites remain
824×420×2 + 512×512×2 = 1,216,448 bytes of PSRAM, excluding allocator overhead
and the shared worker's separate canvas/scratch. Only bounded retry state is new.
UI and existing serial FT8 navigation use the same dashboard path.

## Checks executed

Run directory: `F:\AI\OrcSDR-TEMP\maps-integration-completion`. Raw logs stay there
outside Git.

- Focused dashboard regression compiled with WSL g++17 and the recording display.
  The initial command omitted the existing clock-status dependency and failed
  linking; the corrected command reproduced the missing automatic retry in
  `ft8-map-recovery-red2-20261025.log`.
- Optimized focused dashboard regression: PASS,
  `ft8-map-recovery-green-20261025.log`.
- Same dashboard regression with ASan/UBSan, including busy-holder retry,
  recovery without a revision, worker out-of-memory recovery, stable healthy
  updates and unchanged full-screen-clear count: PASS,
  `ft8-map-recovery-sanitized-20261025.log`.
- `python .local/maps/check_ft8_map_recovery_20261025.py`: real P4 compiler PASS;
  FT8 update stack 96 bytes. Compiler log
  `ft8-map-recovery-p4-20261025.log`.
- `git diff --check`: PASS.

Native build of source `9344fce7` and artifact inspection subsequently passed:

- `python .local/maps/build_release_ft8_map_20261025.py build` — exit 0,
  `release-ft8-map-native-build-20261025.log`.
- `python .local/maps/inspect_release_ft8_map_20261025.py` — PASS,
  `release-ft8-map-native-inspection-20261025.log`. Artifact and hash manifest:
  `.local/maps/release-ft8-map-9344fce7-20261025/manifest.json`.
- Existing 40 KiB internal reserve, production catalog, PSRAM placement, pinned
  engine and symbol checks passed inspection. This candidate also includes the
  previously prepared SD-capacity, LoRa label-redraw and start/pause fixes.

At that source checkpoint, COM17 transition/recovery was unverified and no reset,
tuning change, reclaim command or flash had been issued. Subsequent device
results follow below. COM39 remains untouched. This is not a claim that maps
can allocate through arbitrary exhausted memory or that live buffers may be
discarded; the required hardware gate is the reported ordinary mode transition.

## First hardware candidate

App-only candidate `9344fce7` was installed on COM17 with all non-app partitions
verified unchanged and startup captured. Three LoRa→FT8 returns rendered the
world map, released retained LoRa buffers to zero, and left reception running.
The first released 15,840,000 bytes; free PSRAM afterward was 14,213,036 bytes.
Final shared frame: ready, 524,288 bytes, one tile, 478 features, zero missing or
failed tiles. The hash-verified final PNG visibly contains the world basemap,
receiver marker, attribution and controls. Logs:

- `ft8-map-recovery-transitions-device-20261025.log`
- `ft8-map-recovery-final-20261025.png`
- `ft8-map-recovery-snapshot-20261025.log`
- `release-ft8-map-boot-native-monitor-20261025.log`
- `release-ft8-map-preservation-20261025.log`

However, the second/third LoRa entries reported `RTL_IQ_ERROR no_psram_buffer`
and retained only 8,160,000 bytes, so these are not three successful full LoRa
reception cycles. Inspection found generic dashboard navigation queued LoRa's
capture allocations before `draw_sdr_screen` called FT8's sprite cleanup.
The follow-up moves that existing FT8 cleanup ahead of the LoRa queue call.
This follow-up requires native and round-trip hardware validation before the
overall memory handoff is closed. No protocol decode success is inferred from
these UI/memory checks.

## Final handoff candidate: PASS on COM17

Source `6f625f66` includes `9344fce7` plus the cleanup-order correction.
Native build and artifact inspection passed, followed by a COM17 app-only flash
with previous-app verification, fresh non-app recovery backups and preservation
verification. App SHA-256:
`b314ac3e1ba8e70158e53a3d49f00a18162d2914f2d282723e4a05cafe09de31`.
App size 5,300,944 bytes; factory headroom 990,512 bytes. The 40 KiB internal
reserve and C6 firmware remain unchanged.

After startup, three round trips used the named serial actions
`RTL_UI OPEN LORA`, `RTL_LISTEN LORA`, `FT8 OPEN`, `FT8 BAND 20m`, and
`FT8 TAB MAP`, with read-only status checks between phases. **All three pass:**

- LoRa retained its complete 15,840,000-byte capture allocation on every entry.
- Every FT8 Map return released the inactive LoRa allocation to zero and
  produced a ready world frame with zero missing/failed tiles.
- No `RTL_IQ_ERROR`, manual `RTL_MEM RECLAIM`, or reset between transitions.
- All three final driver checks showed streaming, zero drops and zero overruns.
- Final free PSRAM 14,491,428 bytes; largest free block 14,155,776 bytes.
- The hash-verified final screenshot was inspected: world geography, receiver
  marker, attribution and controls are present. Physical user feedback on the
  first candidate confirmed a visible map but requested less detail for FT8's
  stationary view. That visual simplification is a separate follow-up.

Commands and raw logs, saved in the invocation directory outside Git:

- `python .local/maps/build_release_ft8_handoff_20261025.py build` — PASS;
  `release-ft8-handoff-native-build-20261025.log`.
- `python .local/maps/inspect_release_ft8_handoff_20261025.py` — PASS;
  `release-ft8-handoff-native-inspection-20261025.log`.
- `python .local/maps/flash_release_ft8_handoff_20261025.py` — PASS;
  `release-ft8-handoff-{verify-previous,preservation}-20261025.log`.
- `python .local/maps/com17_label_bench.py .local/maps/ft8-handoff-transitions-plan-20261025.json ft8-handoff-transitions-device-20261025.log` — completed.
- `python .local/maps/verify_ft8_handoff_20261025.py` — PASS; machine-readable
  `ft8-handoff-results-20261025.json` verifies all six allocation states and
  all three ready-frame/streaming checks.
- `python .local/maps/snapshot_ft8_handoff_20261025.py` — PASS;
  `ft8-handoff-snapshot-20261025.log`, `ft8-handoff-final-20261025.png`.

The reported missing-map mode-transition blocker is closed for this tested
COM17 V3c configuration. This is not a long soak, protocol decode acceptance,
all-dongle validation, or approval to release the entire candidate. Retained
map SD tests and other review/matrix gates remain separate. COM17 is left on
FT8 Map at the 20 m preset; serial monitoring is closed. No COM39 access occurred.
