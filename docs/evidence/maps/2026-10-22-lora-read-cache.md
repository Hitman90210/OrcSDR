# LoRa detailed-map refill: bounded read-cache investigation

## Scope and source

Continue from the accepted LoRa dragging/flicker correction. Reduce detailed
refill latency without silently reducing map detail, changing radio scheduling
or replacing the shared renderer. COM17 only, USB serial 30:ED:A0:E2:E6:95.
COM39 untouched. OrcMaps pin remains b59d47819cc3b81c1bbe50ed75b775275bb0218e.

- 6981f4b5: worker yield/wait diagnostics, no scheduling change.
- 78208970: optional 256 KiB application compressed-byte read cache.
- eaf08721: leave preview-allocation headroom before optional cache allocation.
- bbefd8c7: runtime construction of the polymorphic PSRAM cache.
- cb1b567a: reserve preview space only when publication needs a new allocation.

Changed code is confined to the shared map service/cache, map tests and serial/SD
references. No renderer, RTL driver, DSP, C6, partition layout or map-data change.
The cache is above the pinned OrcMaps ByteSource interface; no engine is copied
or vendored. Caller-owned aligned PSRAM storage, fixed 64 block descriptors,
no internal-DMA fallback, complete invalidation on attachment change/rescan/reclaim.

## Diagnostic baseline

6981f4b5 builds and boots with exact-ELF monitoring. App 5,159,904 bytes;
40 KiB internal reserve and existing symbol gates pass. Two affected UI tests,
four P4 syntax checks (three UI units plus map_service) pass. Four matched serial
pans under LoRa reception complete with no reported driver drops/overruns:

| Generation | Tiles | Detail ms | Read ms | Worker wait ms | Yield count |
|---|---:|---:|---:|---:|---:|
| 3 | 4 | 6200.649 | 1473.720 | 830.757 | 925 |
| 4 | 6 | 6883.462 | 1734.899 | 878.388 | 994 |
| 5 | 4 | 6182.462 | 1470.075 | 817.702 | 930 |
| 6 | 6 | 7118.011 | 1820.007 | 890.728 | 1009 |

Wait is wall time surrounding vTaskDelay(1), including rescheduling, already
included in other timings. No pure CPU-time subtraction is claimed. Both versions
must be compared in the same postboot AUTO-gain LoRa configuration, not against
the earlier long-running manual-gain capture. These samples are not a p95 or
physical finger-to-display measurement.

## Host/source checks

Four affected CTest cases pass: map_read_cache, map_pipeline, map_view and
lora_map_controls. Real-archive pipeline test passes, including four byte-identical
842x330 cached/uncached renders (initial, pan, return, closer zoom) and the existing
bounded global quality comparisons. Cache ASan/UBSan passes. Tests cover unaligned
and multiblock reads, collisions, EOF/overflow, short-read retry, archive replacement,
no-buffer fallback and zeroed-storage placement construction. No full suite ran.

The first attempt to build the new target fails because the existing Makefile
build directory had not been reconfigured. Explicit CMake configure then build
and CTest pass. Both failed and corrected logs are retained.

## Device rejection and recovery

78208970 is written and preservation-verified but never booted. Inspection catches
missing preview headroom; eaf08721 supersedes it while COM17 remains in bootloader.
Fresh non-app backups are reused only after complete byte verification before
and after the second write. eaf08721 boots, but opening the LoRa map fails:
Load access fault, MEPC 0x480c603e, MTVAL 0x10. The exact ELF decodes this to
Pipeline::Source::Valid dispatch from PmTilesReader::Open. The new global ReadCache
lives in external BSS/NOLOAD; its constant vtable initializer is cleared rather
than restored by dynamic initialization. This is a new integration-object lifecycle
bug, not evidence of corrupt map data or an SD-card fault.

The failed map test and reboot are not counted as a pass. Raw registers and
stack are retained in lora-refill-cache-postflash-device-20261022.log; decoded
addresses in lora-refill-cache-fault-decoded-20261022.log. The crash dump and current
NVS are backed up under .local/maps/lora-refill-rollback-20261022 before restoring
6981f4b5. Preservation verification passes; its matching native monitor reaches
Ready with SD/Wi-Fi/RTL ready. No erase or C6 update is performed.

bbefd8c7 explicitly constructs ReadCache in begin(), after external BSS startup
and before worker creation. The focused placement/read test and ASan/UBSan pass;
its native build and artifact inspection pass. Hardware acceptance of this
corrected candidate must be recorded below, not inferred from these tests.

## Commands and logs

All commands run from F:\AI\OrcSDR-TEMP\maps-integration-completion unless
specified. Log files remain beside invocation. Native monitor logs live beside
the monitor invocation under apps/orcsdr-tab5, using the exact archived ELF.

```powershell
python .local/maps/build_pilot_20261022.py build lora-refill-worker-timing-20261022
python .local/maps/inspect_lora_refill_worker_20261022.py
python .local/maps/flash_lora_refill_worker_20261022.py
python .local/maps/com17_label_bench.py lora-refill-worker-baseline-plan-20261022.json lora-refill-worker-baseline-device-20261022.log
python .local/maps/build_pilot_20261022.py build lora-refill-read-cache-20261022
python .local/maps/build_pilot_20261022.py build lora-refill-cache-preview-budget-20261022
python .local/maps/inspect_lora_refill_cache_final_20261022.py
python .local/maps/flash_lora_refill_cache_final_20261022.py
python .local/maps/restore_lora_refill_worker_20261022.py
python .local/maps/build_pilot_20261022.py build lora-refill-cache-placement-20261022
python .local/maps/inspect_lora_refill_cache_placement_20261022.py
python .local/maps/flash_lora_refill_cache_placement_20261022.py
```

Targeted host commands:

```bash
cmake -S apps/orcsdr-tab5/tests/maps -B .local/maps/labels-host
cmake --build .local/maps/labels-host --target orcsdr_map_read_cache_tests orcsdr_maps_tests orcsdr_map_view_tests orcsdr_lora_map_tests -j2
ctest --test-dir .local/maps/labels-host -R "^(map_read_cache|map_pipeline|map_view|lora_map_controls)$" --output-on-failure
ORCSDR_LABEL_TEST_ARCHIVE=/mnt/f/AI/OrcSDR-TEMP/orcmaps-quality-delivery/.local/global-assessment/global-z8-candidate.pmtiles ctest --test-dir .local/maps/labels-host -R "^map_pipeline$" -V
g++ -std=c++17 -fsanitize=address,undefined -fno-omit-frame-pointer -g apps/orcsdr-tab5/tests/maps/map_read_cache_tests.cpp -I apps/orcsdr-tab5/ui -I /mnt/f/AI/OrcSDR-TEMP/orcmaps-quality-delivery/include -o .local/maps/read-cache-placement-asan-20261022
.local/maps/read-cache-placement-asan-20261022
```

Logs: lora-refill-worker-*, lora-refill-read-cache-*, lora-refill-cache-* and
lora-refill-rollback-*; build wrapper retains fit-order-native-<suffix>-20261020.log.

## Corrected hardware candidate

bbefd8c7 passes map opening and four pan completions without another reset.
Missing/failed tiles remain zero; radio drops/overruns remain zero. Memory
pressure is materially different from the first diagnostic run: PSRAM free
695176, largest 638976. The optional cache correctly stays disabled because the
conservative policy reserves another preview even though one is already allocated.
All four frames report capacity_bytes=0, so their 5.78–6.81 second timings are
NOT cache performance evidence. This is native direct-read fallback coverage.
Logs: lora-refill-cache-placement-{postflash,baseline}-device-20261022.log.

cb1b567a retains preview priority but reserves extra space only for an absent or
wrong-sized preview. A completed same-sized preview therefore permits cache
allocation on a later frame without another framebuffer allocation. Its native
build and artifact inspection pass. Cache-enabled results from the next checkpoint follow below.


## Cache-enabled COM17 result: cb1b567a

App-only installed and byte-verified, with fresh mutable-partition backups and
verified unchanged world/PHY/bootloader/table. Exact-ELF native boot passes.
App 5,161,520 bytes; factory headroom 1,129,936 bytes; 40 KiB internal reserve
unchanged. App SHA256 f55deb7c30059f0e137b543941020c9f5f60f0a1313503199332323d3992dcdf;
ELF SHA256 1cf431fd878f8ce33e8c264b76c75879b2fcc96b8f25f505892f983315caa590.

The same four +/-120-pixel pans, receiver-centered initial camera E7
440628866,-1230160273, zoom 9/source 8, AUTO gain, active LoRa at 906875000 Hz:

| View | Direct fallback total ms | Cached total ms | Direct read ms | Cached read ms | Cached physical read bytes |
|---|---:|---:|---:|---:|---:|
| 4 tiles / 5444 features | 5911.126 | 4264.183 | 1505.726 | 414.951 | 163840 |
| 6 tiles / 5763 features | 6551.345 | 3732.755 | 1681.357 | 478.085 | 200704 |
| 4 tiles / 5444 features | 5776.609 | 3929.732 | 1469.826 | 373.307 | 163840 |
| 6 tiles / 5763 features | 6808.887 | 4408.259 | 1774.896 | 565.787 | 200704 |

All four frames: ready, missing=0, failed=0, capacity_bytes=262144. Four-tile
frames use 490 block hits / 40 misses, six-tile frames 539 / 49. Logical pipeline
bytes remain 700178 / 767298; caching reduces physical bytes by about 74–77% in
these repeated views. Median frame duration is 4096.958 ms versus 6231.236 ms
in the direct-fallback run (observed reduction 34.25%). The earlier diagnostic
run median is 6542.056 ms. These are four-sample observations, not a p95,
universal speed claim, isolated CPU multiplier or physical touch-latency result.
Live RF/DSP activity differs between runs; for example, decoder memory pressure
is higher during the direct-fallback run. The cache's byte reduction is directly
measured, but the entire total-time difference cannot be attributed solely to it.

The accepted cb1b567a cache-cold first-frame capture records
3837.145 ms, 494 hits / 98 misses, 401408 physical bytes. The earlier 6643.059 ms
frame belongs to bbefd8c7 with capacity=0 and is not a cached cold-frame result.
Image push remains about 26–29 ms in the four accepted cached samples.

Eight rapid alternating pan actions, then zoom in/out and return, pass. The
latest camera finishes at the original E7 point/zoom, with no missing/failed
map tiles. Reclaim after a controlled receiver stop and Home navigation releases
exactly 1373584 bytes from Map frame (two 555720-byte sprites plus 262144-byte
cache). Reopening the LoRa map rebuilds both map and cache; active stream resumes.
Driver reports no drops/overruns. A native CRC-valid encrypted packet is decoded
in the corrected-candidate boot observation; this is live traffic, not a new
controlled Heltec transmitter test.

## Retained-memory SD test and final device state

The first scratch probe waits without receiving its expected response; a delayed
SD_GET_ERROR radio_busy appears in the next capture. The test correctly stops
before issuing a write because absence is unverified. Reception is then paused.
The final path and its .part/.bak variants are verified absent before creating:

/orcsdr/maplabels_330606cb_readcache_cb1b567a_20261022.bin

16384 deterministic bytes write, sync/finish and read back identically, SHA256
6289fbf5c81cf6f5cfffcdbd2551eecee6eb345d4880002ff401a6dda32c2ce3. The cache/frame
remain retained. This verifies paused serial write/readback and subsequent
reception recovery, not simultaneous bulk transfer and active RF. Active-RF map
SD reads and LoRa logger readiness are separate passing checks.

The paused plan's SD portion passes, but its final stream assertion fails because
the harness used RTL_START, which is not the supported receiver-start command.
The attempt to set gain while idle returns ESP_RTL_SDR_ERR_NOT_CLAIMED. No firmware
correction is made for a test-harness mistake. A separate final restore uses the
documented RTL_LISTEN LORA, waits for streaming, restores original manual gain
445 with ESP_OK, returns LoRa Map and passes every final assertion.

Final: STREAMING 906875000 Hz, manual gain 445, effective_sps=954420, drops=0,
overruns=0; LoRa log ready=true/error=false/queued=0/dropped=0. Camera E7
440628866,-1230160273 zoom9; saved receiver point unchanged. PSRAM free=1808372,
largest=1703936; internal free=15135, largest DMA=7680; main stack high-water
mark=1080 bytes. Uptime advances normally, reset_reason=11 remains unchanged
after the accepted candidate's monitored boot. Serial handle is closed.
The owned 16 KiB scratch file is retained for audit; existing SD files are preserved.

Actual commands (integration root), including failed SD plans and final recovery:

```powershell
python .local/maps/build_pilot_20261022.py build lora-refill-cache-retained-preview-20261022
python .local/maps/inspect_lora_refill_cache_retained_20261022.py
python .local/maps/flash_lora_refill_cache_retained_20261022.py
python .local/maps/com17_label_bench.py lora-refill-cache-postflash-plan-20261022.json lora-refill-cache-retained-postflash-device-20261022.log
python .local/maps/com17_label_bench.py lora-refill-cache-baseline-plan-20261022.json lora-refill-cache-retained-baseline-device-20261022.log
python .local/maps/com17_label_bench.py lora-refill-cache-cancel-zoom-plan-20261022.json lora-refill-cache-cancel-zoom-device-20261022.log
python .local/maps/com17_label_bench.py lora-refill-cache-reclaim-plan-20261022.json lora-refill-cache-reclaim-device-20261022.log
python .local/maps/com17_label_bench.py lora-refill-cache-sd-plan-20261022.json lora-refill-cache-sd-device-20261022.log
python .local/maps/com17_label_bench.py lora-refill-cache-sd-paused-plan-20261022.json lora-refill-cache-sd-paused-device-20261022.log
python .local/maps/com17_label_bench.py lora-refill-cache-final-restore-plan-20261022.json lora-refill-cache-final-restore-device-20261022.log
```

Accepted baseline/cancellation/reclaim/final-restore plans exit 0; both SD plans
exit 1 for the explicitly recorded reasons. Final host CTest affected cases pass
4/4 (lora-refill-cache-final-host-20261022.log). The corrected byte-cache and
placement tests pass ASan/UBSan. All artifacts and comparison.json are under
.local/maps/lora-refill-cache-cb1b567a-20261022. Native monitor log:
apps/orcsdr-tab5/lora-refill-cache-retained-boot-native-monitor-20261022.log.

## Remaining gate and next milestone

Physical improvement/flicker acceptance of cb1b567a is requested separately and
pending. No main merge or push. No worldwide native pressure audit or full suite
was run. Existing FT8/SSTV/LoRa storage formats and their write implementations
are unchanged; this is not a claim that every historical SD failure is resolved.

The detailed view is still multi-second. Next measure and reduce repeated tile
decoding/drawing, ideally reusing unchanged tile work, while bounding PSRAM and
preserving pixels, cancellation, radio continuity, preview priority and ownership.
Do not remove cooperative yields based on this result, reduce detail silently,
or claim representative 33 ms interaction/instant detailed refill is complete.
