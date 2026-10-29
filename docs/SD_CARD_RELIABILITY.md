# SD card reliability and regression checks

October 25 release source correction: filesystem capacity now comes from
ESP-IDF 5.5.4 `esp_vfs_fat_info`, rather than raw card sectors and a hardcoded
zero used count. Unmounted/query errors and invalid free-greater-than-total
results return zero usable capacity. Map downloads, SSTV archive checks,
catalog initialization and Settings free-space display use a direct free-space
query, avoiding subtraction of separately sampled total/used values.

Focused capacity fault-injection, real-P4 syntax, the existing transfer test
(including low-space rejection before HTTP), and six shared-SD host programs
pass. These are host/source results: no near-full real card or new firmware
installation is claimed. The capacity preflight is not a reservation; another
writer may consume space during a transfer, so existing write/sync/close checks
remain required. A capacity-query failure does not erase a staged download.
See [release review evidence](evidence/maps/2026-10-25-pr211-review-triage.md).

October 23 source-only follow-up: the map download/recovery worker's existing
16 KiB PSRAM transfer buffer now requests MALLOC_CAP_CACHE_ALIGNED, matching the
shared writable File buffer policy. Size, lifetime, allocation-failure behavior,
SHA-256 checks and activation/recovery checks are unchanged. Real P4 syntax for
map_downloads.cpp passes; log maps-transfer-buffer-p4-syntax-20261023.log in the
integration root, command python .local/maps/check_transfer_buffer_syntax_20261023.py
map_downloads.cpp. This does not prove the previous pointer was unaligned or
explain/fix the historical 12-minute installation. Runtime timing remains open;
no native build/flash or full-map verification was performed for this change.

October 23 replay-cache candidate f38b8601: COM17 passes a 16 KiB owned scratch
write/SHA-256 readback while the completed map and 702,110-byte tile replay stay
retained. Reception is paused because the existing serial file-transfer guard
rejects active reception with radio_busy; the initial attempt stopped before
upload. Reception resumes afterward, drops/overruns remain zero, and LoRa logging
stays healthy. This does not qualify simultaneous RF file transfer or all cards.
[Exact candidate, commands, hash and logs](evidence/maps/2026-10-23-feature-replay-integration.md).

October 22 LoRa/map follow-up: after correcting decoder task startup in
`7c48e358`, COM17 successfully reads its 881-byte RF packet CSV with SHA-256
verification while the completed 555,720-byte map frame remains retained.
Reception is stopped for this read and resumes afterward; this does not establish
simultaneous RF/SD behavior. Logging reports no error or dropped records. Free
PSRAM remains tight, so the general pressure gate stays open.
[Device identity, exact commands, hashes and logs](evidence/maps/2026-10-22-lora-worker-startup.md).

This is the shared record for Tab5 SD changes. Map availability, successful
stdio writes, filesystem durability, adequate DMA memory and RF continuity are
separate checks. A passing screenshot download does not qualify the whole card.

## Failures already investigated

| Area | Finding and correction | Evidence to preserve |
|---|---|---|
| Shared high-volume writes | Unaligned PSRAM fell into single-sector copies; writable streams now use optional 32 KiB cache-aligned PSRAM buffers. Removing buffering is not a fix. | [Write-performance investigation](TAB5_SD_PERFORMANCE.md); `f1588da5` |
| FT8 log | OrcDial bridge consumed scarce internal DMA memory. Queue placement recovered headroom; file append semantics were corrected. FT8 stages CSV in PSRAM and waits for its DMA threshold. | [2026-10-08 FT8 entry](ft8/CHANGELOG.md#2026-10-08--decode-log-to-the-sd-card-works-internal-dma-memory-recovered); `431b1f46` |
| LoRa persistence | An open buffered log needs fflush plus fsync to commit data/metadata. Shared File::sync does both; worker syncs journal and CSV at five-second intervals and when logging stops. Close/sync errors remain observable. | `eaa51423`, `d7deb59a`; [history behavior](lora-history.md), [device checks](lora-history-validation-2026-10-16.md) |
| SSTV archive | Archive worker uses its own cache-aligned stdio buffers, retained until fclose. Existing tests exercise missing/full/flaky/slow storage, partial reads/writes, retry and temporary-file recovery. | `ui/sstv_archive.cpp`; `tests/sstv_archive_test.cpp` under the Tab5 app |
| Map-retained SD read | Historical cache-sync invalid-address error preceded SD error 0x102. SDK's alignment-only check can admit non-DMA CPU heaps. Candidate adds a reachability/range check before using direct DMA. | [Source investigation](evidence/maps/2026-10-17-sd-retained-source-result.md); [5c6f2853 build/device checks](evidence/maps/2026-10-17-sd-guard-device-result.md) passed within their bounds |

These are different failure mechanisms. Reducing map memory or slowing SD is not
a universal correction. The old recorded pointer is unavailable, so the precise
historical map-read allocation remains unproven even though the unsafe-buffer
path is reproduced in a host test.

## Requirements for future SD changes

- Keep Tab5 card on Slot 0, C6 on Slot 1, existing pins/LDO4 and no-format mount.
  Do not reinitialize the Hosted controller while mounting the card.
- Check DMA reachability and cache alignment, including the complete byte range.
  CPU-accessible TCM/RTC is not equivalent to DMA-capable memory. Keep valid
  aligned PSRAM on the direct path; use the SDK copy path for incompatible buffers.
- Preserve aligned stdio buffering and its lifetime through fclose in both the
  shared storage wrapper and SSTV archive. Allocation failure keeps default libc
  buffering; do not add another permanent internal DMA pool without measurements.
- Preserve append mode for logs. A new record must not truncate earlier records.
  Short writes, sync/close errors, full/missing cards and incomplete records must
  remain visible. A write counter alone does not establish durable storage.
- Keep SD work off decoder/audio callbacks. Preserve FT8 staging, LoRa's logging
  queue and SSTV's archive worker. Record free and largest internal DMA blocks,
  PSRAM, queue drops and RF counters under actual load.
- Keep CRC-valid reception, archive contents and reconstructed NODEINFO/position
  state distinct. Do not alter old formats or erase packs to make a test pass.
- Test files must use an explicitly reserved scratch path that is verified absent
  before creation. Existing SD self-check/benchmark commands use fixed names and
  delete them; do not run them on valuable cards without checking those names.

## 2026-10-19 map-download staging source checkpoint

The new transfer path uses the shared `File` wrapper and its existing buffering,
sync and DMA guard; it does not bypass those protections with direct SDMMC or
raw stdio writes. Its caller-owned transfer buffer must be PSRAM on P4. The
40 KiB internal reserve and existing FT8/LoRa/SSTV write paths are unchanged.

One additive operation, `File::truncate`, is shrink-only and is used to restore
an owned staging file to its last synced resume checkpoint. It flushes, refuses
extension, invokes SDK FAT/VFS `ftruncate` and restores the cursor. It does not
truncate logs, existing map packs or user-selected files. No card contents were
changed while preparing/testing this source.

Two alternating checked resume records prevent a torn newer record from being
the sole recovery source. Bad responses are rejected before changing a staging
tail. Completion requires full SD read-back hashing plus an actual PMTiles
header/root check. Prepared files remain in a hidden subdirectory outside flat
map discovery; version replacement still needs a recovery transaction.

The focused host transfer harness and shared DMA test each pass 1/1; three P4
syntax checks pass. The host storage adapter flushes stdio and injects faults;
it does **not** execute real FAT fsync/truncate or establish power-loss durability.
Physical-card behavior, retained-map pressure, largest internal DMA block,
radio/logging continuity and controlled interruption remain separate gates.
See [transfer evidence and exact logs](evidence/maps/2026-10-19-map-transfer-staging.md).

## 2026-10-17 COM39 device checkpoint

`5c6f2853` builds and boots; retained-map read/write and real LoRa CSV readback
pass. Before flashing, old 99affb1b already showed logging error=true and 33
lost log records despite a healthy RTL stream. The new cycle ends logging
ready=true/error=false/dropped=0; 411 CSV rows include two new RF records and
preserve all 409 earlier rows. This does not recover records previously dropped.
The historical rejected pointer is unrecorded; do not treat this as proof of
all past failures or all-mode SD reliability. [Commands, actual counters,
limitations and logs](evidence/maps/2026-10-17-sd-guard-device-result.md).

## Repeatable host gate

From the repository root on Linux/WSL:

```bash
bash tools/test-sd-storage.sh
```

This builds/runs five focused test programs with AddressSanitizer and UBSan:
DMA/range/alignment, benchmark write-size plan, LoRa archive/cache/CSV behavior,
SSTV archive fault handling, and the actual FT8 log flush function extracted
from main.cpp with injected write/sync/close failures. Build/run logs and a summary stay in the
invocation directory; names carry an explicit UTC timestamp. It does not access
COM ports or physical SD cards. Temporary host binaries are retained in the
directory printed by the runner. A failure stops the gate and identifies its log.

The SD storage workflow runs this gate on relevant pull requests and main pushes
and saves logs even after failures. The workflow is prepared locally; no GitHub
run or required branch-protection check is claimed until published/configured.
The buffer test is also registered as `sd_dma_buffer` in the maps CTest project.

Actual 2026-10-17 local result: **4/4 PASS**, including the SSTV slow-card test.
[Host summary](evidence/maps/2026-10-17-sd-storage-host.log).
The first GCC compile found a missing initializer_list include in the new test;
that portability issue was corrected before this successful run.

These tests do not execute ESP-IDF SDMMC, hardware allocation, or the LoRa
worker's real fsync. The FT8 harness executes its production flush function
against fault-injected file operations, not a physical card.

On 2026-10-18 the FT8 fault test reproduced an existing false-success path:
a failed close still advanced the written count and discarded staged rows.
FT8 now checks the CSV header, data write, fsync and close before clearing its
stage; failures retain rows and report their actual operation. All five host
programs pass. Hardware validation of this additional change is pending.
A partially committed append can be duplicated on retry; this change does not
make CSV appends transactional or qualify sudden power loss/card removal.

## Device acceptance before declaring reliability

The October 19 coordinated install tests now join staging, durable intent,
activation, ownership commit and journal finalization. They exercise uncertain
commits and interrupted replacement, plus repeated A/B artifact reuse. That last
case reproduced a stale completion marker; preparation now retains obsolete
backup metadata in `/orcmaps/.recovery/<manifest-sha256>.bak` before reuse, with
collision checks and no automatic history deletion. Existing map-worker recovery
uses a 16 KiB PSRAM buffer and cooperative yields, without holding its render
mutex. This is source/host/P4-syntax evidence, not physical SD/NVS durability.
[Coordinator commands, logs and open gates](evidence/maps/2026-10-19-map-install-coordinator.md).

October 19 map installation follow-up: hidden staging now has journaled
activation/recovery source. Focused host tests cover interrupted first install
and replacement, corrupted journals/bytes, status errors and external metadata
conflicts. Recovery runs before PMTiles discovery and fails visibly rather than
treating SD I/O errors as absent files. Previous archives and unmanaged maps are
retained. These are host/P4-syntax results, not physical SD power-loss proof;
installer hardware validation and worker/UI wiring remain open.
[Exact commands, logs and limitations](evidence/maps/2026-10-19-map-activation-recovery.md).

| Check | Current candidate status |
|---|---|
| Normal native link/boot and unchanged reserve/firmware identity | PASS on COM39, 5c6f2853, full 40 KiB reserve |
| Full map retained, repeated checksum-verified SD reads | PASS: three 2.76 MB reads, frame 1,417,728 bytes retained, no reclaim |
| Bounded scratch-file writes/readback with retained map | PASS: unique absent-checked 131,109-byte file, exact readback |
| Internal DMA/PSRAM, navigation and LoRa continuity during maps | Bounded checks PASS; DMA margins remain tight; one abort after intentional reception stop is recorded |
| SSTV archive save/reload with map memory pressure | Host faults pass; candidate device behavior untested |
| FT8 append/flush/readback and staged-row handling under pressure | Historical validation exists; candidate device behavior untested |
| LoRa append/sync, stop/readback, reboot replay with NODEINFO preserved | Two new RF rows read back; old 409-row prefix intact; 56 archived nodes replayed. Reboot of the two new records not repeated |
| Card full/removal and power-cut durability | Separate destructive/fault testing needed; not authorized by a normal flash |

For each device cycle save target/firmware hash, card/bus details, commands,
timings, data hashes, memory/counters, actual failures and restoration steps.
Preserve recovery/NVS/SD data; do not fold an overnight soak or card fault
campaign into a bounded test approval. No guarantee against all future SD failures
is inferred from a host gate or one device sample.

## 2026-10-20 label/dashboard retained-map check

COM17 `30:ED:A0:E2:E6:95`, firmware `af877ed7`: after visiting LoRa, ADS-B and
the picker, both map buffers retained 2,835,456 B. An existing 8,192-byte reserved
scratch file was read with reception stopped and matched its previously recorded
SHA-256; no new SD file was written. Reception resumed at BROWSE 146.520 MHz,
about 2.4 MSPS, zero observed driver drops/overruns in this bounded check.

Free PSRAM after this workload was 1,870,944 B, largest block 917,504 B. The
memory-holder report showed 15,840,000 B of reclaimable LoRa IQ buffers. Treat
this as workload/reclaim evidence, not a leak diagnosis or a universal SD pass.
FT8 with these retained allocations still needs its native fallback/reclaim test.
Commands/logs and point-in-time DMA/stack measurements:
[evidence](evidence/maps/2026-10-19-geographic-label-controls.md).

## 2026-10-22 retained-map SD check and separate FT8 fault

COM17 `30:ED:A0:E2:E6:95`, app `050442b0`: a uniquely named, absent-checked
65,536-byte scratch write/readback passes with map memory 751,376 bytes and
LoRa IQ memory 15,840,000 bytes retained. Matching SHA-256, memory readings and
logs are in the [hardware checkpoint](evidence/maps/2026-10-22-map-dashboard-hardware-checkpoint.md).
Reception was stopped for transfer; this is not an active-RF SD test.

The subsequent return to FT8 caused a stack-protection fault during serial
snapshot output. Thus the overall plan fails despite its passing SD substeps.
The preserved dump does not establish SD corruption, an allocation root cause
or an ordinary main-task stack overflow. COM17 was recovered and returned to
FT8; investigate this retained-memory dashboard transition before claiming the
combined workload reliable. No power-loss/card-removal acceptance is implied.

Follow-up `89109160` reconstructs the interrupted main task and removes a
6,840-byte automatic FT8 comparison snapshot from its stack, storing it in
PSRAM. Native build/placement and COM17 app-only preservation pass; five serial
retunes with active FT8 waterfall and retained LoRa/map memory pass without
reset. This is a drawing-stack correction, not an SD driver change. No new SD
transfer was run for it; retained PSRAM is still tight and broader combined
workload acceptance remains open. Exact results, including stale test-assertion
failures and corrected read-only final checks, are in the checkpoint above.

## October 22 optional map read-cache candidate

The shared map worker can retain up to 256 KiB of compressed archive bytes in
64 aligned 4 KiB PSRAM blocks. This is a read-only application cache above the
existing FileByteSource/libc/SD guards; it does not change mount, stdio buffering,
DMA reserve, writes or logging. The archive is immutable during attachment.
Discovery/rescan, archive changes, memory reclaim and failure cleanup invalidate
and release it. Required frame/preview storage takes priority; reserve preview memory if a new
preview must be allocated, while reusing a correctly sized existing preview. An
absent/failed cache allocation keeps direct reads. No buffer is allocated from internal/DMA memory.

Short backing reads are not cached as complete blocks and retry storage on the
next request. Focused tests cover collisions, EOF/overflow, short-read recovery,
source replacement, no-buffer fallback and identical rendered pixels. Native
performance and retained-memory SD/reclaim results must be recorded separately;
these source checks do not establish that historical card failures are solved.

The deployed cb1b567a COM17 checkpoint passes active-LoRa map reads, exact
1,373,584-byte frame/cache reclaim and rebuild, and a paused 16 KiB uniquely
named scratch write/readback with map memory retained. LoRa reception/logging
then resume without reported drops/errors. A PSRAM object-vtable initialization
fault in the first cache candidate is captured and corrected; it is not classified
as SD corruption. The test harness's unsupported RTL_START command is also
recorded and corrected to RTL_LISTEN LORA during final recovery. All failures,
commands, hashes, logs, memory figures and limits are in the
[read-cache evidence](evidence/maps/2026-10-22-lora-read-cache.md).


## October 24: clean GitHub download baseline

The [PNY 32 GB microSDHC U1 benchmark](evidence/maps/download-benchmarks/2026-10-24-com17-pny32/README.md) records source/card settings, 481 monotonic status samples, controlled pause/resume and complete 543,380,195-byte installation. Transfer took 19m40.3s; verification/activation 21m21.2s. Reception started partway through verification, with IQ activity, so this is not an idle-card speed comparison. The timed installer log has no SD read/write errors; restart discovery and offline regional rendering pass. Initial stale-mount/read errors after the card swap and three post-reboot driver drops/overruns remain separately recorded. No new blanket SD-reliability claim or attribution to buffer alignment follows from this single run. Original SD data and C6 were preserved.

## October 26: LoRa logger recovery

The logger now retains its current record across open/write/sync failures,
retries every two seconds, and commits only after both journal and CSV sync.
Retry rollback is restricted to that record's known append suffix; older data
and unknown extra writes are protected. The bounded queue can still overflow
during sustained storage failure, and pending RAM does not survive power loss.

`python3 tests/lora_log_recovery_test.py` passed 19 focused cases. COM17 then
encountered a real open error and recovered automatically without losing or
duplicating the held record; the old CSV prefix remained identical. Native
placement, preservation checks, logs and limits are recorded in the
[repair evidence](evidence/maps/2026-10-26-lora-logger-recovery-fix.md).
