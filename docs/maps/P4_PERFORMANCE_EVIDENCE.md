# ESP32-P4 map performance evidence


## Latest storage checkpoint: retained-map SD checks on COM39

Firmware `5c6f2853` builds/boots with the full 40 KiB reserve. Three 2.76 MB reads
and a unique 131,109-byte scratch write/readback pass with the map frame retained;
no reclaim. LoRa CSV readback adds two RF rows and preserves all 409 old rows.
Logging ends ready/error=false/dropped=0. Before flashing, old firmware already
reported error=true/dropped=33; those lost records are not recovered. Tight DMA
headroom, live SSTV/FT8 candidate checks and regional-SD concurrency remain open.
This is a bounded device result, not all-mode durability qualification or a soak.
COM17/C6 untouched; no push/merge. [SD results and logs](../evidence/maps/2026-10-17-sd-guard-device-result.md).
[Shared storage regression reference](../SD_CARD_RELIABILITY.md).

## Earlier display checkpoint: OrcMaps v0.2.3 and readable footer on COM39

Firmware `99affb1b` boots with OrcMaps v0.2.3 (`dc6bc12`) and the full 40 KiB
DMA reserve. Three device map crops match the previous renderer exactly; stored
location fields match. User confirms enlarged text is readable and controls good.
Loaded redraw samples take 1.41-2.42 seconds; return to the starting view takes
1.91 seconds, so no consistent speed multiplier is established. Driver drops and
overruns are zero at sampled checks; one candidate CRC-valid LoRa packet observed.
HOME's 33 ms setting yields p50 38/p95 43 ms in a short sample. Internal/DMA
headroom and held-map-memory SD reliability remain open. COM17/C6 untouched.
No push or merge; integration acceptance remains incomplete.

[Device results, commands and logs](../evidence/maps/2026-10-17-v023-device-result.md).
[Preceding host/build verification](../evidence/maps/2026-10-17-v023-upgrade-result.md).

## Previous device checkpoint: navigation and changed-location save tested on COM39

Normal firmware `04898891` (implementation `ef756048`) builds, flashes and boots.
Closer starting zoom, pan arrows, prominent map button and changed-location
save/display/reboot persistence pass the bounded COM39 checks. Original live point
restored. The user confirms controls and coarser world detail look good.

Active-LoRa redraw samples take 1.5-2.7 seconds, with zero sampled driver
drops/overruns. Final passive observation records 18 CRC-valid packets and nine
decoder failures (causes uninvestigated), not a controlled decode/soak qualification.
Held-cache SD read passes again; earlier intermittent SD/capture issues and tight
internal/DMA margins remain open. Regional/dashboard/recovery/release acceptance
is incomplete. COM17/C6 untouched; no merge or push.

[Device results, commands and logs](../evidence/maps/2026-10-17-picker-navigation-device-result.md).
[Preceding host verification](../evidence/maps/2026-10-17-picker-navigation-save-host.md).
Earlier sections are chronological evidence, not the latest completion claim.


Status: baseline only. No optimization result is claimed.

## Measured source/device baseline

| Metric | COM17 | COM39 |
|---|---:|---:|
| Uptime ms | 143725176 | 14898322 |
| Free heap bytes | 17677000 | 2502180 |
| Internal free bytes | 44663 | 37455 |
| Largest internal block | 31744 | 31744 |
| Internal DMA free | 10699 | 3587 |
| Largest DMA block | 10240 | 1728 |
| PSRAM free bytes | 17642988 | 2468300 |
| Largest PSRAM block | 17301504 | 2359296 |
| Active mode | Airband | LoRa |

Different firmware/runtime/modes mean these are not comparable performance samples. No device was reset or paused to normalize them.

Existing retained main-code build artifact from the preceding local session is 4,617,600 bytes, versus 6,291,456-byte factory capacity (1,673,856 bytes partition headroom; 1,411,712 bytes below the release limit after its 256 KiB reserve). That is artifact size evidence, not proof of live flash hash. Fresh ESP-IDF 5.5.4 baseline build passed with the same 4,617,600-byte application size. Rollback binary/ELF are retained at `.local/maps/baseline.bin` and `.local/maps/baseline.elf`; full command log is `maps-baseline-build.log`.

SD throughput, map frame/render phases, PMTiles lookup/inflate/MVT times, display flush, SD seek/read amplification, loaded-RF map FPS, allocation-failure behavior and physical visual acceptance: UNTESTED in this session. No throughput benchmark is run on COM17's historical SD card without permission.

OrcMaps host baseline: CMake Debug build in `.local/baseline-host`; ctest 1/1 target passed. This does not establish ESP32-P4 timings. Existing OrcMaps docs contain historical static-render measurements; those must not be presented as this integration's measurements.

## Experiment protocol

Use deterministic PMTiles golden fixtures and fixed viewports. Record archive/source/license, target adapter, viewport and cold/warm state. Instrument lookup, inflate/decode, feature render and flush separately, plus internal DMA free/largest, PSRAM, app size, RF/audio/IQ drops, USB overruns and watchdogs. Test missing/corrupt pack, manifest/hash mismatch, allocation failure, antimeridian and zoom bounds.

Prototype one measured bottleneck at a time. Promote only a reproducible gain without RF regressions. Do not add speculative cache/dirty rectangles/pools before baseline evidence. Read-only host fixtures first; flash, pack provisioning and SD writes need separate user approval once artifacts are reviewable.

## Host Springfield render baseline

The released OrcMaps pack-inspect pipeline rendered the tracked 3,507,636-byte Springfield PMTiles at 44.0500/-123.0220, zoom 14, orcsdr-dark to `.local/springfield-baseline.ppm`. Reported host times in ms: enumerate 4.674; lookup 106.396; decompress 8.038; decode 15.362; translate 6.643; classify 0.780; render 5.532; image write 10719.703. Concurrent native build and Windows/WSL file I/O make this unsuitable for a speed comparison. No optimization is promoted from it. Log: OrcMaps worktree `maps-pack-inspect-baseline.log`.

Fresh baseline application SHA256: `0f3f95656394080133ce1f74ed8408cb8a1031d9cb3414f021a9ee7e40b9470b`. This is a host-built rollback artifact, not a newly flashed device image.

## First integrated worker build (2026-10-17)

Source: shared map worker, region/legacy adapter, and LoRa caller; OrcMaps pinned
to `70fea367909c74794bb69c7aed97e0f11f0a9387` (v0.2.1 miniz fix plus reviewed PR4).
`& ./apps/orcsdr-tab5/tools/build-tab5-idf.ps1` passed after correcting two
compiler errors. Log: `maps-worker-native-recheck.log` in the invocation directory.
Application: 4,741,456 bytes (`0x485950`), factory headroom 1,550,000 bytes;
release guard headroom 1,287,856 bytes. SHA-256:
`bf6d2a57e6ab85852c063f1750c00abab5313a0702ad81248f3e36bfd430f9a1`.
Linked-image symbol gate: PASS, two bundled miniz symbols, none ROM-absolute.
No device flashed; active-RF render timing, memory, visual correctness, cancellation
and allocation-failure recovery remain UNTESTED on COM17/COM39.
The worker uses one private PSRAM sprite, a depth-one latest-request queue and
per-feature cancellation/yield hooks. UI sprite transfer stays on the UI owner.
No map performance optimization has been claimed from these build results.

## Allocation-safe candidate build

Approved full native rebuild: `& ./apps/orcsdr-tab5/tools/build-tab5-idf.ps1 -WorldMap F:/AI/OrcSDR-TEMP/maps-integration-completion/.local/maps/world/world-overview-z4.pmtiles`.
Final corrected run log: `maps-allocation-final-build.log` (invocation directory),
reviewed copy `docs/evidence/maps/2026-10-17-allocation-final-build.log`.

- Application: 5,020,576 bytes; factory headroom 1,270,880 bytes; release guard
  headroom 1,008,736 bytes. Fresh-baseline application growth: 402,976 bytes.
- SHA-256: `395c18dea8cad6d7a1454a9ac88fd6e2d5508b5e8373d1eebf8af499121a678f`.
- C++ exceptions enabled; emergency allocation reserve configured to 1,024 bytes.
  This is a runtime/build change, not a measured speed improvement. All actual
  device heap/DMA/PSRAM impacts remain UNTESTED with this image.
- Requested/locked/fetched OrcMaps agreement: PASS at c3cc09de3882149e03c7c00d7897c6882b324c6b.
- Linked-image miniz check: PASS; two bundled symbols, none ROM-absolute.
- All other locked dependencies, including RTL-SDR, Hosted, wifi_remote and mdns,
  compare equal to the pre-refresh lock. ESP-IDF stays 5.5.4.
- `cmd /d /c .local\maps\run-pipeline.cmd`: 3/3 host targets pass with ASAN and
  exception unwinding, including injected allocation failure followed by recovery.
  Log `maps-allocation-recovery-host.log`; reviewed copy under docs/evidence/maps.

The first exception-enabled build completed but retained the old engine lock despite
its new manifest. It is not the candidate for the cleanup fix. A new configure gate
rejects this mismatch; the recorded gate fails against the stale lock and passes
against the corrected lock/fetched component. Resolution also tried to advance two
unrelated floating dependencies; that build was stopped and their exact prior lock
records restored. The final build above contains only the intended OrcMaps update.

The new frame/inventory adapters catch bad_alloc, free the failed frame and allow a
new request/rescan. Device directory/text-file handles close through exception-safe
ownership. Host failure injection is not a substitute for ESP32 allocation-failure
acceptance. No device reset, flash, SD change, radio pause, or profile run occurred.

## Bounded hardware acceptance plan (awaiting target approval)

1. Confirm COM39's physical identity. USB serial observed E8:F6:0A:E2:D6:03;
   COM17 is 30:ED:A0:E2:E6:95. Port naming alone is insufficient authorization.
2. Stop reception for an approved backup/flash window. Save an opaque full-flash
   recovery image and its hash locally; do not decode or print private NVS values.
3. Provision only the reviewed bootloader, table, app and world archive at their
   stated offsets. Do not erase the chip, write NVS, modify SD, or update C6.
4. Capture boot/version/self-check, stored location and map inventory before and
   after. Existing legacy setup must bypass first-run and preserve saved location.
5. Exercise Settings picker Cancel / explicit Confirm, wizard Back / Skip / Cancel /
   Quick Start / Finish, optional Network return, keyboard and touch. Reboot after
   progress checkpoint to test recovery only with explicit device-test authorization.
6. On active LoRa (current COM39 reception), pan/zoom and alternate Home, maps and
   dashboard for a bounded session; collect RTL_MAP_STATUS, RTL_HEALTH, RTL_MEM STATUS,
   RTL_STATS and UI status before/after. Record map total/lookup/stream/render/I/O/push
   timing, cached pushes, PSRAM and largest DMA block alongside drops/USB/reset data.
7. Exercise missing/corrupt pack, no-SD and allocation failure only in an approved
   controlled setup. Do not alter COM17's historical packs or remove either card
   while workers use it. FM audio continuity needs its own short receiving sample;
   no new overnight soak is required by this plan.

`stream_us` includes decoder work, lookup/read work inside streaming, and worker yields;
it is not a disjoint CPU category. Cached pushes count reuse of the single completed
frame, not a rendered-tile LRU hit rate. `bytes/reads/io_us` describe archive reads,
not measured SD bus throughput. Exact exclusive decode/SD throughput, p50/p95 frame
intervals, active-RF continuity, physical screen correctness, and low-memory recovery
are UNTESTED until target logs establish them. No optimization has been promoted.

## Final location-safe candidate

The focused safety review found a separate interrupted-location-save issue. After
its single-record persistence fix, four host targets pass (`maps-location-after.log`),
including the interruption regression that failed beforehand. Incremental native
commands: from the exported ESP-IDF environment,
`cmake --build apps/orcsdr-tab5/build-native-hosted3 -- -j4`.
Both `maps-location-native-build.log` and `maps-candidate-final-build.log` completed
successfully; the last run includes the new record in legacy-device classification.
The linked miniz gate passes. The pin gate and unchanged-dependency checks also pass.

Final app: 5021344 bytes; factory headroom 1270112 bytes;
release guard headroom 1007968 bytes. SHA-256:
`bd4000ae7c29d9608eddc7e03d2288ba65c101bb964c2274509fbd3248680fe1`.
`docs/evidence/maps/2026-10-17-candidate-manifest.json` records this final candidate;
the allocation-only build above is historical intermediate evidence. No flash,
physical interrupted-save test or active-RF profile has occurred.

## COM39 acceptance attempt and recovery — 2026-10-17

The user confirmed COM39 / USB serial `E8:F6:0A:E2:D6:03` and approved candidate
`4971ee5b`, world provisioning, a reception pause, recovery backup and bounded
UI/radio checks. COM17 (`30:ED:A0:E2:E6:95`) was not opened or flashed in this attempt.
No C6 update, SD overwrite, NVS erase or whole-chip erase was performed.

Pre-flash commands used an existing authenticated key without `PAIR`; `RTL_STOP`
completed with `RTL_STOP_RESULT ESP_OK`. `SD_LIST` found the existing FM settings,
their backup and `/orcsdr/lora_packets_v2.csv` (42177 bytes at inventory time).
`RTL_LOCATION STATUS` reports the temporary lookup result, **not canonical saved
location**; the saved coordinate was not obtained before the failed boot. Do not
claim a physical location-migration test passed.

Recovery command (Python is the IDF 5.5 environment):
`python -m esptool --chip esp32p4 --port COM39 --baud 921600 --after no_reset read_flash 0x0 0x1000000 .local/maps/com39-recovery/20261017-1726/full-flash.bin`.
Result: 16777216 bytes read successfully in 139.1 seconds. SHA-256:
`90b19ec8fe53ff1f807ba8f68e9872cb36601de6aa07a42bc356ad86c06e3751`.
Opaque recovery/NVS/core-dump files remain ignored locally; no private NVS was
decoded. Partition comparison showed every existing partition unchanged and only
the new world partition added at `0x650000`.

The explicit COM39 `write_flash` command used the four offsets listed above.
Bootloader, table, application and world each reported `Hash of data verified`.
Before application boot, `verify_flash` of the opaque saved NVS at `0x9000` and
saved core dump at `0x610000` both reported `verify OK (digest matched)`.

**Hardware acceptance FAILED.** The candidate rebooted before `app_main` with:
`main_task: Could not reserve internal/DMA pool (error 0x101)` followed by `abort()`.
ESP-IDF was reserving the unchanged 40 KiB internal/DMA pool. No map worker, setup
flow or active-RF map profile ran. The first monitor command also hit an independent
host `esp-idf-monitor` 1.9.0 Logger error with `--disable-address-decoding`; retrying
without that flag captured the device startup failure. These are separate failures.

The candidate startup dump was saved separately, then original bootloader,
partition table, factory region and prior core dump were restored from the backup.
All four restoration writes passed hash verification; the unchanged NVS again
passed `verify_flash` before boot. The unused world bytes remain in flash, but the
restored partition table does not expose them as an installed map.

Recovery checks: `UI_SELF_CHECK` returned all six fields equal to 1. Subsequent
`RTL_DRIVER STATUS` reported STREAMING, 906875000 Hz, effective_sps=959967,
bytes=290455552, overruns=0 and drops=0. `RTL_LORA_STATS` reported events=2,
crc_ok=2, messages=2, failures=0 and drops=0. This is real LoRa reception on the
restored firmware, not map candidate acceptance. The previously connected LoRa
antenna was not physically changed during the test.

At restored uptime 160903 ms: internal_free=36487, internal_largest=31744,
dma_free=2627, dma_largest=2432, psram_free=2127968,
psram_largest=2097152. This snapshot is not an overnight soak or memory-leak test.

Logs were saved in the worktree root, the invocation directory:
`maps-com39-preflash-20261017.log`, `maps-com39-rom-identity-20261017.log`,
`maps-com39-backup-20261017.log`, `maps-com39-recovery-check-20261017.log`,
`maps-com39-flash-20261017.log`, `maps-com39-preservation-20261017.log`,
`maps-com39-boot-20261017.log`, `maps-com39-boot-retry-20261017.log`,
`maps-com39-startup-dump-20261017.log`, `maps-com39-restore-20261017.log`,
`maps-com39-recovery-nvs-20261017.log`, `maps-com39-restored-health-20261017.log`
and `maps-com39-restored-final-20261017.log`. Reviewed copies/extracts are under
`docs/evidence/maps/2026-10-17-com39-*`.

### Prepared placement adjustment — not flashed

`ui/map_service.cpp` places the 340-byte canvas object and 256-byte archive-path
buffer in PSRAM. They are UI/worker state, not DMA descriptors or ISR state.
Both objects' linked addresses move from internal `0x4ff...` to PSRAM `0x485...`;
`.dram1.bss` shrinks from 171300 to 170708 bytes, recovering 592 bytes after
alignment. The DMA reserve, exception reserve, radio code and other locked
dependencies are unchanged. The startup failure itself is confirmed; whether this
extra margin is sufficient is **UNTESTED on hardware**.

Exact incremental test: from the exported IDF environment,
`cmake --build apps/orcsdr-tab5/build-native-hosted3 -- -j4`.
Result: PASS, 10 build steps; linked miniz gate PASS; application remains 5021344
bytes. `python .local/maps/check_startup_placement.py` passed symbol-placement and
positive internal-BSS-recovery assertions. Logs:
`maps-startup-placement-build-20261017.log` and
`maps-startup-placement-check-20261017.json`. The initial placement assertion assumed
all 596 payload bytes would shrink BSS; section alignment gives 592, so the check
was corrected to verify actual placement and positive recovery.

Prepared application SHA-256:
`91ed8cef4026447684b7f7f8ce81eb8c8a1bef7e2feb486d5e732acfc573a02d`.
Historical `4971ee5b` binary/ELF remain `.local/maps/candidate.*`; the new artifacts
are `.local/maps/startup-placement.*`. No repeated full rebuild or full regression
suite was run. Next gate: approve the new candidate's COM39 flash, verify boot
first, then resume the bounded setup/location/map/radio acceptance sequence.

## COM39 e87c6d3c attempt — 2026-10-17, 18:14 local

The user explicitly approved flashing `e87c6d3c` and bounded boot/setup/map/radio
checks. Source HEAD and app SHA-256 were rechecked, and COM39 again matched USB
serial `E8:F6:0A:E2:D6:03`. Reception stopped with `RTL_STOP_RESULT ESP_OK`.
Fresh opaque NVS/core backups are in `.local/maps/com39-recovery/e87-20261017/`;
the original full recovery image remains unchanged. No COM17 or C6 access occurred.

Commands: the same explicit COM39 four-region `esptool ... write_flash` procedure,
followed by `verify_flash 0x9000 .../nvs-before.bin 0x610000 .../coredump-before.bin`.
Result: four flash hashes PASS; both preservation digests PASS. Native
`esp_idf_monitor --port COM39 --no-reset --target esp32p4` used the actual RISC-V
toolchain prefix, and an authorized monitor reset captured startup.

**Boot FAILED again.** On-device ELF identity is `268b6336a...`; one logged internal
region grew from `0xA3D0` (41936) to `0xA620` (42528) bytes, confirming the 592-byte
placement improvement reached the device. Nevertheless, ESP-IDF's unchanged
40960-byte DMA reservation still returned `ESP_ERR_NO_MEM`. The decoded abort is
`main_task` at ESP-IDF `components/freertos/app_startup.c:179`, before `app_main`.
The placement improvement is insufficient. The actual DMA free/largest immediately
before reservation was not logged, so the allocation causing the shortfall is not
yet isolated. Do not claim the exception reserve or map canvas is the proven cause.
No setup, map render, dashboard compatibility or map performance test ran.

The new crash dump was saved before restoring original bootloader, table and factory
image, plus the freshly saved pre-attempt core dump. Four restoration flash hashes
PASS; fresh NVS verification PASS. The old partition table again hides the unused
world bytes. Original SD data and settings were not overwritten.

Recovery checks: `UI_SELF_CHECK` returned six 1 fields; `RTL_CAPTURE_STATUS` returned
running. At uptime 206637 ms, `RTL_DRIVER STATUS` reported STREAMING at 906875000 Hz,
effective_sps=959913, bytes=361725952, overruns=0, drops=0. `RTL_LORA_STATS` reported
events=2, crc_ok=2, messages=2, failures=0, drops=0 on the restored firmware.
Memory then: internal_free=36459, internal_largest=31744, dma_free=3467,
dma_largest=3328, psram_free=2127988, psram_largest=1933312 bytes.

Invocation-directory logs: `maps-com39-e87-preflash-20261017.log`,
`maps-com39-e87-backup-nvs-20261017.log`, `maps-com39-e87-backup-core-20261017.log`,
`maps-com39-e87-flash-20261017.log`, `maps-com39-e87-preservation-20261017.log`,
`maps-com39-e87-boot-20261017.log`, `maps-com39-e87-startup-dump-20261017.log`,
`maps-com39-e87-restore-20261017.log`, `maps-com39-e87-recovery-nvs-20261017.log`,
`maps-com39-e87-restored-health-20261017.log`,
`maps-com39-e87-restored-final-20261017.log`. Reviewed extracts/copies are saved in
`docs/evidence/maps/2026-10-17-com39-e87-*`. Private backups and full memory dumps
remain local, not committed.

### Prepared opt-in startup diagnostic — no image built or flashed

`ui/map_startup_diagnostics.cpp` wraps only
`esp_psram_extram_reserve_dma_pool` to report requested bytes and free/largest DMA
immediately before/after forwarding to the real function. The function's argument
and result are unchanged. ROM output avoids dynamic stdio initialization; its plain
format literals add no writable static storage. This is observation, not a fix.

`main/CMakeLists.txt` excludes it by default. The build helper adds `-MapStartupDiag`
and explicitly passes OFF on normal invocations, avoiding a sticky diagnostic cache.
There is no exception-pool, DMA-reserve, stack-size, receiver or DSP change.

Narrow checks actually executed, from the worktree root:

- `python .local/maps/check_startup_diag_syntax.py`: PASS using existing ESP32-P4
  compiler flags, syntax only.
- `python .local/maps/check_startup_diag_object.py`: PASS, standalone P4 object,
  zero writable allocated sections and no malloc/new/global-constructor dependencies.
  Its only undefined symbols are the two heap queries, `esp_rom_printf`, and the real
  reservation function. This does not validate final link wrapping or runtime output.
- PowerShell `Language.Parser::ParseFile` on `build-tab5-idf.ps1`: PASS, no parse errors.
- `git diff --check`: PASS.

Logs: `maps-startup-diag-syntax-20261017.log`, `maps-startup-diag-object-20261017.log`,
`maps-startup-diag-helper-20261017.log`. The compiler retains the existing SDK
literal-suffix warning; there are no new compile errors.

Next requested gate is **one** approved diagnostic build and COM39 boot capture,
then restoration. Intended build command:
`& ./apps/orcsdr-tab5/tools/build-tab5-idf.ps1 -MapStartupDiag -WorldMap F:/AI/OrcSDR-TEMP/maps-integration-completion/.local/maps/world/world-overview-z4.pmtiles`.
Before flash, verify the real reservation call and wrapper symbols in the linked
image, miniz/pin/size gates, and zero new writable diagnostic static storage.
Repeated build/flash attempts require approval under the session maintainer rules.
The next integration milestone remains a bootable map candidate before UI/RF work.

## Approved startup diagnostic — 2026-10-17, 18:26 local

The user approved one diagnostic build from `fa520a3452cb3e78ee458f99408610689f35dd19`,
COM39 capture and restoration. Executed from the worktree root:
`& ./apps/orcsdr-tab5/tools/build-tab5-idf.ps1 -MapStartupDiag -WorldMap F:/AI/OrcSDR-TEMP/maps-integration-completion/.local/maps/world/world-overview-z4.pmtiles`.
Result: PASS; pin and linked miniz gates PASS. App size 5021696 bytes (352 more than
e87c6d3c); SHA-256 `5f3f9ef802e75b70d9ad7e7b7cd426d2a639861e3aa0fc087f82dbcd2ec98640`.
Disassembly confirms `main_task` calls the wrapper, which calls the real reservation
function; internal BSS stays 170708 bytes. This was a test image, not a release.
The pre-flash manifest's `hardware_flashed=false` is its historical creation state.

COM39 identity again matched `E8:F6:0A:E2:D6:03`. `RTL_STOP` completed cleanly, fresh
opaque NVS/core backups were saved in `.local/maps/com39-recovery/diag-20261017/`,
then explicit bootloader/table/app writes each passed flash hash verification.
NVS, the pre-attempt core dump, and the existing world archive all passed
`verify_flash`; world bytes were verified rather than rewritten. COM17 and the C6
firmware were not accessed. The normal `--no-reset` native monitor captured an
authorized reset with the correct RISC-V decoder.

Actual startup output (same values on repeated automatic reboot cycles):

```text
ORC_MAP_STARTUP phase=before reserve_bytes=40960 dma_free=95508 dma_largest=40960 internal_free=135056 psram_free=28028348
ORC_MAP_STARTUP phase=after result=257 dma_free=95508 dma_largest=40960
```

The device has enough total free DMA memory. The first 40960-byte allocation fails
at the reported largest-block boundary, before any reservation changes the counts.
The real allocator returns ESP_ERR_NO_MEM; it is not a map-render, partition-write,
Wi-Fi receive, USB or DSP failure. No maps/setup acceptance ran.

The local SDK source explains the boundary: `heap_caps_get_largest_free_block`
reports the unaligned TLSF fit size, while a DMA allocation goes through hardware
alignment and `tlsf_memalign_offs`, which needs additional space for alignment and
metadata. Internal cache-line alignment in this configuration is 64 bytes. The SDK
sources inspected for this conclusion are unmodified locally:

- [ESP-IDF v5.5.4 heap alignment](https://github.com/espressif/esp-idf/blob/v5.5.4/components/esp_mm/heap_align_hw.c)
- [ESP-IDF v5.5.4 TLSF allocation](https://github.com/espressif/esp-idf/blob/v5.5.4/components/heap/tlsf/tlsf.c)
- [ESP-IDF largest-block API contract](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32p4/api-reference/system/mem_alloc.html#_CPPv432heap_caps_get_largest_free_block8uint32_t)

A separately found [esp-rtl-sdr startup RAM note](https://github.com/hardcoreerik/esp-rtl-sdr/blob/master/CHANGELOG.md)
describes an earlier small-BSS-change crossing this same startup boundary. It is
background evidence only; the pinned driver was not changed or upgraded.

### 32-bit allocator reproduction

Command: `cmd /d /c .local\maps\run-tlsf-repro.cmd`. Result: PASS with 32-bit MSVC
using the local SDK's TLSF source. Observed output:

```text
region=42528 metadata=784 raw_largest=41736 reported_largest=40960 plain_40960=PASS aligned64_40960=FAIL
region=44000 metadata=784 raw_largest=43208 reported_largest=43008 plain_40960=PASS aligned64_40960=PASS
```

This reproduces the allocator mechanism; it does not simulate the complete P4 heap
layout, ROM implementation, startup allocations or device performance. This test
region was empty except for its own TLSF metadata. Assertions require 32-bit pointers
and size_t, an ordinary 40960-byte allocation that succeeds, failure with 64-byte
alignment in the smaller region, and aligned success in the enlarged region.

Credit/license: SDK TLSF is Matthew Conte's BSD-3-Clause implementation. SDK code
was not vendored into OrcSDR or modified in place. The ignored local test copy uses
three explicit char-pointer casts for GNU void-pointer arithmetic, plus an MSVC
adapter for the compiler attribute/leading-zero builtin and the disabled weak
poisoning hooks. The independently written experiment is retained with the evidence.
The first host attempts exposed a forced-include path issue, GNU C portability and
weak-hook linking issues; these were harness failures, then corrected. Logs preserve
those attempts locally. They are not device regressions or passing firmware tests.

### Restoration and remaining gate

The diagnostic crash dump was saved. Original bootloader, partition table and factory
image, plus the fresh pre-attempt core dump, were restored with four verified flash
hashes. Fresh NVS verification passed before the restoration reboot. The world bytes
remain unexposed by the restored old partition table. `UI_SELF_CHECK` returned all
six fields equal to 1; reception returned to LoRa 906875000 Hz.

At restored uptime 421833 ms, the driver reported 733052928 bytes, effective_sps=959978,
overruns=0 and drops=0. LoRa: events=3, crc_ok=3, messages=3, failures=0 and drops=0.
Memory: internal_free=36527, internal_largest=31744, dma_free=3623, dma_largest=3584,
psram_free=2127996 and psram_largest=2097152. This is restored-firmware evidence.

Invocation-directory logs: `maps-startup-diag-build-20261017.log`,
`maps-startup-diag-link-20261017.json`, `maps-startup-diag-disassembly-20261017.log`,
`maps-com39-diag-preflash-20261017.log`, `maps-com39-diag-backup-nvs-20261017.log`,
`maps-com39-diag-backup-core-20261017.log`, `maps-com39-diag-flash-20261017.log`,
`maps-com39-diag-preservation-20261017.log`, `maps-com39-diag-boot-20261017.log`,
`maps-com39-diag-startup-dump-20261017.log`, `maps-com39-diag-restore-20261017.log`,
`maps-com39-diag-recovery-nvs-20261017.log`, `maps-com39-diag-restored-health-20261017.log`,
`maps-com39-diag-restored-final-20261017.log`, `maps-startup-tlsf-repro-20261017.log`.
Reviewed copies/extracts are in `docs/evidence/maps/2026-10-17-*`; opaque private
backups and complete memory dumps remain ignored locally.

### Prepared map-state placement change — no new image built

Four task-owned payloads move to PSRAM: ADS-B RegionalView (488 bytes), LoRa map
FrameInfo (416), shared location Snapshot (312), and map identity buffer (256).
Total payload: 1472 bytes. The critical-section lock stays internal; callers of the
shared location snapshot are task/UI code. The change adds placement attributes and
required includes only; no receiver, projection, persistence, DMA-reserve,
exception-reserve or radio/DSP algorithm changes. Main.cpp has exactly one annotation
change, checked byte-for-byte against HEAD, preserving its existing NUL bytes.

`python .local/maps/check_map_placement_probe.py`: PASS, compiling the four actual
P4 types after correcting the ELF inspection to skip its FILE symbol. Also,
`maps-map-placement-probe-result-20261017.log` confirms all four payloads occupy
external BSS. `maps-map-placement-source-check-20261017.log` confirms the sole
main.cpp edit. The 32-bit allocator model predicts the added margin permits the
full aligned reservation; actual linked internal-memory recovery, local-static
initialization overhead, boot, map worker memory and active-RF operation remain
UNTESTED for this change. No second firmware build or flash was run after the
approved diagnostic attempt.

Next requested gate: build the placement change with the startup diagnostic enabled,
confirm actual linked recovery and a successful 40960-byte reservation, then run the
bounded setup/location/map/radio acceptance sequence on COM39. Roll back on boot
failure. The diagnostic remains opt-in and normal helper invocations explicitly turn
it off. Main is not merged; hardware acceptance must pass first.
