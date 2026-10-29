# Offscreen rejection: native candidate and 20-pan result

## Result

Source **77a8cdce**, engine **6a621a70ebbf7906b1d6c089bd61dbbc456475ed**,
COM17 **30:ED:A0:E2:E6:95**. **The p95 ≤2-second detail-refill gate fails**:

| Worker stage | p50 | p95 | Max |
|---|---:|---:|---:|
| Total refill | 2.645144 s | 3.148265 s | 3.188228 s |
| Replay/stream | 0.966094 s | 1.028424 s | 1.038961 s |
| Render/flush | 1.605977 s | 2.080498 s | 2.080538 s |
| Tile I/O | 0 | 0 | 0 |

The earlier `137e244a` run had total p50 2.865801s / p95 2.890615s. Median
improves in this observation while the tail worsens; do not claim an overall
speed improvement or an isolated multiplier. Mainline source, memory/load and
gain differ across observations. This does not meet the agreed acceptance gate.

## Build, flash and startup

```text
python .local/maps/build_offscreen_20261024.py build
python .local/maps/inspect_offscreen_20261024.py
C:/Espressif/python_env/idf5.5_py3.14_env/Scripts/python.exe .local/maps/flash_offscreen_20261024.py
C:/Espressif/python_env/idf5.5_py3.14_env/Scripts/python.exe .local/maps/monitor_offscreen_20261024.py
```

All exit 0. Component manager resolves the exact pin; existing miniz gate passes.
App size **5,289,280 bytes**, headroom **1,002,176 bytes**. App SHA-256
`b16f87f7db54c43b7aaf72763096e03fcae844f90cdab1d8ae4711a90254ae78`;
ELF `fd224dc013d3c9e746e89dd6d0f5c22bdfb2affccff1f910e5ed71499a2f63a9`.
The 40 KiB internal reserve, 1 KiB exception reserve and existing PSRAM placement
checks pass. Numeric progress and the new verification-cost observer are included.

Prior accepted app `c552111f` verified before write. Fresh NVS/PHY/coredump/world
backups and boot/table/app verification pass after the app-only write. No SD
pack, location, C6, bootloader or partition-table update; COM39 untouched.
Rollback/artifact manifest: `.local/maps/maps-offscreen-6a621a7-20261024/`.
Native monitor matches ELF prefix `fd224dc01` and reaches SD/Wi-Fi/RTL Ready.
The first Wi-Fi attempt reports reason 2, then connects via existing retry.
Monitor was closed before the authenticated command test.

## Workload and checks

```text
python .local/maps/com17_label_bench.py maps-offscreen-refill-final-plan-20261024.json maps-offscreen-refill-final-device-20261024.log
python .local/maps/summarize_offscreen_refill_20261024.py
```

Both complete successfully. Exactly 20 distinct completed generations; offsets
`(40,0), (0,20), (-40,0), (0,-20)` repeated five times. Same receiver-centered
842×330 LoRa map, labels off, six tiles and 5,763 features each; camera zoom 9,
source zoom 8. All measured pans have six replay hits, 702,110 retained bytes,
zero backing reads and zero missing/failed tiles. Initial cold view was not a
pan sample. Nearest-rank percentiles use worker wall time, including scheduling;
they do not measure gesture-to-display latency or continuous cached movement.

LoRa streaming at 906.875 MHz: 955,716 samples/s before pans, 958,097 afterward,
zero drops/overruns. Driver gain varies 254→328, so this is not fixed-RF-load
isolation. No decoder acceptance is claimed. Logger ready, error=false,
queued/dropped=0. Loaded PSRAM free 876,772 initially / 2,810,088 finally, largest
block 819,200 / 1,933,312. Largest DMA block 22,528 at both snapshots; main task
stack high-water mark 1,432. No new reset during measurement; uptime advances.
Saved coordinates match the pre-flash observation. Final state is Home,
graphics=1, receiving; the serial port is released.

## Preserved harness failures

The first attempt's opening command was consumed while dismissing startup
splash; it collected no pans. A second attempt reached Home but tried starting
already-running reception and correctly received busy. The final plan explicitly
stops/restarts reception before collecting samples. No source workaround or
additional flash was used. Both failed logs remain. The first summary invocation
was too early and correctly refused a still-live capture; the completed capture
was summarized afterward rather than restarted.

Logs beside invocation:
`maps-offscreen-native-build-20261024.log`,
`maps-offscreen-inspection-20261024.log`,
`maps-offscreen-flash-console-20261024.log` plus per-part logs,
`maps-offscreen-boot-native-monitor-20261024.log`,
`maps-offscreen-refill-device-20261024.log`,
`maps-offscreen-refill-corrected-device-20261024.log`,
`maps-offscreen-refill-final-device-20261024.log`,
`maps-offscreen-refill-summary-20261024.log` (premature), and
`maps-offscreen-refill-summary-complete-20261024.log`.
Machine result: `maps-offscreen-refill-summary-20261024.json`.

## Next gate

The user physically checked this COM17 candidate and confirmed:
"Dragging, colors and controls look correct." This accepts visible behavior;
it does not establish the approximately 33 ms motion timing or pass the failed
two-second detail-refill gate.
Full-file verification counters are compiled/installed but not exercised by
this pan run; no second 543 MB download was performed. Further refill work must
explain the render/flush tail and scheduling variance before another broad
optimization. Engine PR #7 CI is green at the pushed head (P4/S3 links included);
the CodeRabbit check says completed, but actionable-comment triage remains open.
No merge/release or full-goal completion is claimed.
