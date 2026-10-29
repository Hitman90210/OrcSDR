# SD guard: native build and COM39 device results

Date: 2026-10-17, America/Los_Angeles. Tested source:
`5c6f28532c9a8e7baf643a1076b715fc82942a8c`. Engine remains OrcMaps v0.2.3 `dc6bc12`.
The approved single native build/app-only flash/bounded-test cycle passed.
This is not complete integration, all-mode SD qualification, a soak, or a release.

## Scope, artifact and preservation

Production change is the allocation-free predicate in
`apps/orcsdr-tab5/ui/sdmmc_dma_buffer.hpp` and its assignment to the SD-card host
in `ui/orcsdr_storage.cpp`. It checks full DMA-reachable ranges before the SDK's
alignment check, selecting the existing sector-copy path for incompatible CPU
buffers. The shared mount protects both FileSystem and direct stdio callers,
including SSTV; existing aligned 32 KiB buffers, append/sync logic and workers stay.
No SDK/managed component edit, permanent DMA pool, map reclaim, radio restructuring
or C6-host change. Source/test/CI/history files are listed in `git show 5c6f2853`.
[Preceding cause investigation and host tests](2026-10-17-sd-retained-source-result.md).

Native configure/link, dependency agreement, bundled-miniz and partition gates:
PASS. App 5,028,688 bytes, 224 above the previous app;
factory headroom 1,262,768 bytes (20%). App SHA-256:
`4e04eb639531cf462735c6fd7c55d587a41d7658155443cd02a4f8b2018a9e18`. ELF SHA-256:
`a9dfe4ba3d9ed723d373dba12a5876c2669684a02a8fd98c342bade3601e1cc0`. Boot matches `a9dfe4ba3` and keeps the full
40,960-byte reserve. Startup diagnostics OFF. Existing compiler warnings remain.

COM39 USB serial E8:F6:0A:E2:D6:03 verified for esptool operations. Fresh opaque
full-app/NVS/core/boot/table backups and SHA manifest are retained privately at
`.local/maps/com39-recovery/sd-guard-20261017/`. Old app matches saved 99affb1b
before writing; partition table matches the candidate. Only app@0x10000 was
written, with hash verification. All five preservation digests pass: boot, table,
NVS, old core dump and embedded world. No partition/world/SD-pack overwrite,
format, COM17 access or C6 flash. Existing CSV was backed up before flashing.
Normal logging can append SD data and runtime journals can update NVS; preservation
is verified before boot, not a promise of byte-identical NVS after running.

## Existing failure recorded before the flash

Old installed 99affb1b reported LoRa logging requested=true, ready=false,
error=true, dropped=33, while the RTL driver still streamed with drops=0 and
 overruns=0. Old cache had 58 live nodes. The saved CSV had 409 RF records from
56 distinct senders. New boot replays without rejected records and loads 56
archived nodes. This preserves the archived data; it does not recover the old
33 dropped records or unsaved live observations. The exact rejected historical
DMA pointer was not captured, so the guard's matching cause remains a bounded
conclusion, not proof of every past SD failure.

## Retained-map read/write checks

Map: embedded Natural Earth world, 1248x568, frame **1,417,728 bytes**. Retained
frame revision/generation unchanged throughout the transfer phase. Reception and
logging paused for serial file transfers; no RTL_MEM RECLAIM issued.

- Three 2,764,854-byte image reads all pass UART SHA verification and byte-for-byte
  comparison with prior captures (map, zoom, map again). Total 8,294,562 bytes.
- Before writing, target, .part and .bak receive expected absent-file open failures.
  These three expected SD_GET_ERROR open_failed lines are not failed transfers.
- A fresh 131,109-byte deterministic scratch file (128 KiB plus 37 bytes) passes
  upload hash/close/rename and subsequent full readback. The bytes match exactly.
  This exercises full buffered writes and a short final chunk. It is not an SD
  throughput benchmark or power-cut durability test.
- Initial post-flash CSV is exactly the pre-flash 54,790-byte file; later readback
  preserves that complete prefix and adds two RF rows.

[Transfer hashes/counts](2026-10-17-sd-guard-readwrite.json).
Scratch retained intentionally for inspection: `/orcsdr/sdguard_5c6f2853_c92abf1f98e841ca85d1479c706e7e12.bin`. It is the only new
scratch file written, contains no RF/user data, and was not an existing file.
No test overwrote SSTV archives, FT8 logs, LoRa history or map packs.
The map source is the embedded partition; concurrent rendering from regional SD
PMTiles remains unqualified by these retained-memory checks.

## Real LoRa save and final state

V3-class dongle and the previously connected suitable LoRa antenna, US LongFast
906.875 MHz, requested 960 ksps, manual 49.6 dB, RTL AGC/bias off. Natural RF was
uncontrolled; no Heltec transmission or injected test record.

Uninterrupted observation captures a CRC-valid encrypted packet, decoder elapsed
1813 ms, followed by history written=1 and logging error=false/dropped=0. After a
normal stop/logging flush, checksum-verified CSV readback contains **411 rows**,
up from 409; both new rows are RF-origin (from_host=0). The original entire file
is an exact prefix. Final history written=2, error=0. This is actual saved-record
readback, not just a UI/counter claim. [CSV result](2026-10-17-sd-guard-lora-readback.json).
Reboot replay of these two *new* records was not repeated in this cycle.

One capture aborted with reason=stalled immediately after the first RTL_STOP;
decoder drops=1 is retained in evidence. No increment during uninterrupted
observation. This is separate from zero driver drops/overruns and zero logging
drops; the record does not claim all drop counters are zero. Before pausing,
driver effective_sps=958719; uninterrupted end=959539. After the final restart,
952452 sps is an early sample. No spontaneous reset/panic or unexpected cache/SD
transfer error observed in these bounded logs.

HOME -> RF Lab -> HOME and Settings/picker/zoom/pan checks pass. UI_SELF_CHECK
am/fm/home/shortwave/keypad/editor all 1. HOME: setting33 ms; 185 frames/7194 ms,
p50=38/p95=43 ms, rolling128 intervals. Loaded map samples total1.328-2.370 s;
all sampled views missing=0/failed=0. No new renderer speed or all-mode visual
acceptance claim follows from this storage change.

Final COM39: map picker at starting point/zoom, retained frame1,417,728 bytes,
LoRa streaming/manual49.6 dB; logging ready=true/error=false/queued0/dropped0.
No Use Location/save operation; stored settings were not intentionally changed.
Port released after the harness. Final memory snapshot, bytes:

| Pool | Free | Largest block |
|---|---:|---:|
| Internal | 15,187 | 9,728 |
| Internal DMA | 1,107 | 960 |
| PSRAM | 2,077,856 | 1,998,848 |

Internal minimum 791, DMA minimum 231,
main stack high-water 3480. DMA headroom remains tight;
PSRAM snapshots vary with decoder allocation and are not a heap improvement claim.

## Commands, logs and next gate

All commands ran from `F:/AI/OrcSDR-TEMP/maps-integration-completion/`; Python is
`C:/Espressif/python_env/idf5.5_py3.14_env/Scripts/python.exe`.

```powershell
& ./apps/orcsdr-tab5/tools/build-tab5-idf.ps1 -WorldMap F:/AI/OrcSDR-TEMP/maps-integration-completion/.local/maps/world/world-overview-z4.pmtiles
python .local/maps/com39_bench.py .local/maps/sd-guard-preflight-plan.json maps-com39-sd-guard-preflight-20261017.log
python .local/maps/com39_bench.py .local/maps/sd-guard-pause-plan.json maps-com39-sd-guard-pause-20261017.log
python .local/maps/flash-sd-guard.py
python .local/maps/capture_nav_boot.py maps-com39-sd-guard-boot-20261017.log
python .local/maps/com39_bench.py .local/maps/sd-guard-boot-check-plan.json maps-com39-sd-guard-boot-check-20261017.log
python .local/maps/sd_guard_bench.py .local/maps/sd-guard-retained-plan.json maps-com39-sd-guard-retained-20261017.log
python .local/maps/com39_bench.py .local/maps/sd-guard-final-plan.json maps-com39-sd-guard-final-20261017.log
python .local/maps/com39_bench.py .local/maps/sd-guard-observe-plan.json maps-com39-sd-guard-observe-20261017.log
python .local/maps/com39_bench.py .local/maps/sd-guard-log-readback-plan.json maps-com39-sd-guard-log-readback-20261017.log
python .local/maps/com39_bench.py .local/maps/sd-guard-leave-running-plan.json maps-com39-sd-guard-leave-running-20261017.log
```

All operations exit0 and command plans reach PLAN COMPLETE. Root native log:
`maps-sd-guard-native-build-20261017.log`. Fresh recovery logs:
`maps-com39-sd-guard-backup-{app,nvs,core,boot,table}-20261017.log`;
flash/preservation/boot logs use the same prefix. Root reviewed counters:
`maps-com39-sd-guard-reviewed-counters-20261017.log`. Result JSON files and the
native manifest also live in this invocation directory. Reviewed counters,
hashes and build/flash summary are copied beside this report. Raw network/
coordinates/packet contents, CSV/BMP files, helpers, firmware and recovery are
preserved privately under `.local/maps/`; no keys or user payloads are committed.

[Shared SSTV/FT8/LoRa/storage requirements and host gate](../../SD_CARD_RELIABILITY.md)
remain the regression reference. Host gate 4/4 and targeted maps CTest5/5 passed
in the preceding source check; not rerun as a full suite in this device cycle.
Workflow is prepared locally, not a published CI/required-check claim.

Next integration milestone: regional PMTiles discovery and shared-dashboard maps,
with preserved legacy data and concurrent regional-SD/logging checks. Existing
FT8 close-error reporting gap, live SSTV/FT8 candidate save/reload, new-record
reboot durability, card-full/removal/power-cut behavior and tight DMA margin stay
open. No additional soak, flash, push, merge or card-fault campaign is implied.
