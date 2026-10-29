# SD buffer reachability guard: source and host verification

**Later device result:** the approved native/COM39 cycle of 5c6f2853 passed its
bounded checks. [Device results](2026-10-17-sd-guard-device-result.md).
The following source-checkpoint statements are historical.

Date: 2026-10-17, America/Los_Angeles. Parent: `0469331d`.
No native firmware rebuild or flash in this cycle. COM39 remains on `99affb1b`.
This report prepares a narrow candidate; it does not close SD reliability.

## Evidence and cause boundary

Historical `maps-com39-picker-setup-fetch-20261017.log`, lines 28-31, records
`cache: esp_cache_msync(113): invalid addr or null pointer`, followed by
`sdmmc_read_sectors_dma ... 0x102`, `diskio_sdmmc ... 0x102`, and
`SD_GET_ERROR read_failed`. The same cache error appears in the setup log.
The pointer itself was not logged; no exact historical allocation is claimed.

Current installed 99affb1b was checked without reset or test-file writes: stop LoRa,
read existing v23cmap.bmp/v23czom.bmp, verify hashes, resume LoRa/manual 49.6 dB.
Both 2,764,854-byte downloads PASS and match the prior local captures byte-for-byte.
Map frame remains 1,417,728 bytes, revision 9, before and after. No reclaim issued.
This passing baseline shows the intermittent failure is not reproduced merely by
retaining map memory. COM17/C6 untouched, no location or pack changes.
Normal background LoRa logging is distinct from test-file writes; no claim is
made that runtime SD files remained byte-identical.

Local ESP-IDF v5.5.4 source trace:

- `esp_driver_sdmmc/src/sdmmc_host.c:1289`: the default buffer check tests only
  pointer/length alignment, including when a buffer is not DMA reachable.
- `sdmmc/sdmmc_cmd.c:584`: sector reads use that callback to choose direct transfer
  or an existing temporary `MALLOC_CAP_DMA` single-sector copy path. Writes use
  the same callback and corresponding copy path.
- `esp_driver_sdmmc/src/sdmmc_transaction.c:134-146`: direct transfer checks alignment
  again, then invokes cache sync. Aligned TCM/RTC pointers pass the first check.
- `esp_mm/esp_cache_msync.c:113` and `hal/esp32p4/include/hal/cache_ll.h:1076`:
  TCM/RTC addresses fail the cache address-range check and produce the logged error.
- The P4 memory layout allows general CPU allocations in TCM and RTC RAM, neither
  of which is marked DMA-capable. Actual board heap logs include both regions.

This establishes a deterministic unsafe-buffer path consistent with the error,
not proof that the unrecorded historical pointer was TCM/RTC. Fault testing uses
modeled P4 address regions/alignment on the host; it does not execute an SD controller.

## Patch scope

`ui/sdmmc_dma_buffer.hpp` adds an allocation-free buffer predicate: nonempty,
no address wrap, both endpoints in the same internal-DMA or mapped external-DMA
region, plus the SDK's existing alignment check. `ui/orcsdr_storage.cpp` assigns
it only to the Tab5 SD-card host created by mount_tab5_sd. Invalid direct buffers
therefore use the existing SDK sector-copy path; failure to allocate that buffer
still returns the SDK's error. There is no retry loop, map reclaim, extra permanent
DMA allocation, SDK edit, engine dependency change or C6-host callback change.

`tests/sdmmc_dma_buffer_tests.cpp`, two small platform stubs and the maps-test CMake
target cover aligned TCM/RTC, flash, unmapped external memory, internal/PSRAM valid
buffers, cross-boundary ranges, misalignment, empty/null and overflowing ranges.
The test now includes the production predicate directly. Its pre-fix run used the
existing SDK alignment-only predicate and failed on aligned TCM acceptance.

Design follows the DMA/cache contract in the official
[ESP-IDF 5.5.4 P4 SDMMC documentation](https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32p4/api-reference/peripherals/sdmmc_host.html#_CPPv425sdmmc_host_do_transactioniP15sdmmc_command_t).
The existing SDK implementation is Apache-2.0; it was inspected, not vendored or
modified. The guard independently combines public pointer and alignment APIs.

## Commands and actual results

Invocation directory: `F:/AI/OrcSDR-TEMP/maps-integration-completion/`.
Python: `C:/Espressif/python_env/idf5.5_py3.14_env/Scripts/python.exe`.

```powershell
python .local/maps/com39_bench.py .local/maps/sd-retained-read-plan.json maps-com39-sd-retained-baseline-20261017.log
python .local/maps/com39_bench.py .local/maps/sd-retained-resume-plan.json maps-com39-sd-retained-resume-20261017.log
cmake -S apps/orcsdr-tab5/tests/maps -B .local/maps/pipeline-host
cmake --build .local/maps/pipeline-host --config Release --target orcsdr_sd_dma_tests
ctest --test-dir .local/maps/pipeline-host -C Release -R '^sd_dma_buffer$' --timeout 10 --output-on-failure
python .local/maps/check_sd_dma_syntax.py
ctest --test-dir .local/maps/pipeline-host -C Release -R '^(sd_dma_buffer|setup_flow|location_store|map_pipeline|world_geometry)$' --timeout 15 --output-on-failure
```

The MSVC ASAN runtime directory must be on PATH for this existing host build.
Initial red-test launches stalled without it (one terminated, one timed out).
After adding the runtime path, RED failed in 0.03s on TCM, as intended. A first
post-header build reused the old object because the previously absent conditional
include was not tracked; direct inclusion/recompilation corrected that harness issue.
GREEN passed 1/1; final targeted regression passed 5/5 in 1.36s. No full suite.
Real P4 `orcsdr_storage.cpp` syntax check PASS; existing initializer warnings remain.
Syntax-only does not establish link, firmware size, boot or hardware behavior.

Root logs: `maps-sd-dma-host-{config,build-red,red,build-red2,red2,red-runtime,
build-green,green,build-green2,green2,regression}-20261017.log`,
`maps-sd-dma-p4-syntax-20261017.log`,
`maps-com39-sd-retained-{baseline,resume,reviewed}-20261017.log`.
Reviewed red/green/regression and coordinate-free device counter logs are beside
this report. Raw logs and screenshots remain private. Prior backups are preserved.

## Gate at source checkpoint (subsequently tested)

Follow-up at the user's request: reviewed the earlier SSTV, FT8 and LoRa SD work
and documented shared requirements in [SD card reliability](../../SD_CARD_RELIABILITY.md).
The new `tools/test-sd-storage.sh` sanitizer gate passes 4/4: DMA buffers, existing
write-size plan, LoRa history and SSTV archive faults. It preserves the existing
test programs rather than replacing them with one shared success counter.
`.github/workflows/sd-storage-tests.yml` prepares the same gate for relevant PRs
and main pushes, with saved logs; no published CI run is claimed.
Exact local command: `wsl.exe -d Ubuntu -- bash -lc 'cd /mnt/f/AI/OrcSDR-TEMP/maps-integration-completion && bash tools/test-sd-storage.sh'`.
Root summary: `maps-sd-storage-host-fixed-20261017.log`; UTC-named build/run logs
are listed inside it. The first new GCC compile required an initializer_list
include; fixed before the successful run. Expanded buffer tests also preserve
direct-DMA acceptance for aligned 512-byte through 128-KiB PSRAM transfers.

One approved normal native build/link and COM39 app-only flash/boot, followed by
bounded checksum-verified SD reads with map retained, pan/zoom/navigation, memory
and LoRa continuity checks. Preserve NVS, SD packs, world, partition table, C6 and
rollback; COM17 stays untouched. Do not claim the historical intermittent issue
closed without further device evidence. No flash, push or merge occurred here.
