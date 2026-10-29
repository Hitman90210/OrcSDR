# Preview timing and retained-map validation — COM17, October 19

## Approved scope and candidate

User approved the pending native build/COM17 app-only test and OrcMaps full
configured host regression scopes. COM39 remained assigned to Claude for Dead
Air; no access to it occurred in this work.

OrcSDR source: 34d3366ced04cfb9559f9a48f889c32b87fd7a61 plus preserved dirty
wizard/map changes. Preview recorder implementation is d87d1309. The later
download guard is not called by the firmware. Native artifact contains engine
722d0d382f99b2887694af333f85b1ee048f4208, not boundary candidate 3d9d01c.
Source/diff/untracked wizard hashes and linked placement are sealed in:

    .local/maps/preview-timing-34d3366c-20261019/manifest.json

App: 5,061,632 bytes; partition headroom 1,229,824 bytes (20%). SHA-256:
ae0d94b15eacefcce42afa165333490a09836d08d1c0d67bcca89194fcd23515.
Build ID: maps preview timing dirty 2026-10-19. SDK: ESP-IDF 5.5.4.

## Native build and inspection

Invoked from apps/orcsdr-tab5, using the existing saved configuration and C6
image, ESP_IDF_VERSION=5.5 and the pinned toolchain PATH:

    python C:/Espressif/frameworks/esp-idf-v5.5.4/tools/idf.py -B build-native-hosted3 -D "ORCSDR_BUILD_ID=maps preview timing dirty 2026-10-19" -D SDKCONFIG=build-native-hosted3/sdkconfig -D C6_FIRMWARE_BIN=F:/Ai/OrcSDR/.orcsdr-cache/hosted-c6/3.0.6-e1a4f5492ac4-orcdial-393c7ea436e4/esp_hosted_tab5_c6.bin -D ORCSDR_MAP_STARTUP_DIAG=OFF -D ORCSDR_WIFI_RX_DIAG=OFF -D CCACHE_ENABLE=0 build

PASS, 331 steps. Log: apps/orcsdr-tab5/preview-timing-native-build-20261019.log.
Requested/locked/fetched engine pin passes; dependency lock is byte-identical to
the saved prebuild lock. Miniz symbol and 8,192-byte PSRAM FT8 scratch gates pass.
The 40,960-byte internal DMA reserve and 1,024-byte exception reserve remain.
Label pool: 15,416 bytes in external BSS. Each preview timing pool: 6,224 bytes
in external BSS, 12,448 total. Existing worker recorder: 2,072 external bytes.

From integration root:

    python .local/maps/inspect_preview_timing_20261019.py

Inspection PASS; log preview-timing-native-inspection-corrected-20261019.log.
The first inspection failed because prebuild copies had been placed one parent
directory too high. Only that task-created two-file folder was moved to its
intended worktree location after verifying both paths and contents. No rebuild
or device change was needed; original failure log is retained.

## Device identity, recovery and preservation

USB serial 30:ED:A0:E2:E6:95 verified on COM17. Read-only preflight observed
Home/BROWSE at 26,591,754 Hz, driver 0.9.3 streaming near 2.4 MSPS with zero
drops/overruns, C6 3.0.6 matching. Antenna was not independently confirmed; these
are stream/UI tests, not RF decoding acceptance. Logs are in integration root:

- preview-timing-preflight-{plan-20261019.json,20261019.log,run-20261019.log}
- preview-timing-stop-{plan-20261019.json,20261019.log,run-20261019.log}

Reception was paused. From integration root:

    python .local/maps/flash_preview_timing_20261019.py

PASS. The prior app SHA matches saved c2450314… recovery. Fresh NVS, PHY,
coredump and world-partition backups were read and verified before writing.
Only the P4 application at 0x10000 was written. Candidate, bootloader, partition
table and every non-app partition verified afterward. Fresh backups and candidate
BIN/ELF are in the artifact directory above. Original full recovery remains:

    .local/maps/com17-recovery/20261018-155834/

Logs: preview-timing-flash-run-20261019.log and operation-specific
preview-timing-{verify-previous,backup-*,verify-backups,app-write,preservation}-20261019.log.
NVS and SD packs were preserved; C6 was not updated.

Native observation used idf.py -B build-native-hosted3 -p COM17 monitor --no-reset.
One explicit monitor Ctrl+T/Ctrl+R reset started the flashed app; Ctrl+] closed
the monitor before serial control tests. Log:
apps/orcsdr-tab5/preview-timing-boot-native-monitor-20261019.log.
Boot reached SD/Wi-Fi/RTL ready and streamed normally. Wi-Fi retried once then
connected; pre-existing coredump data was reported, not a newly observed panic.
Normal V4 initialization STALLs were followed by RTL_START ESP_OK and streaming.

## Timing result — 33 ms gate NOT met

From integration root:

    python .local/maps/com17_label_bench.py preview-timing-postflash-plan-20261019.json preview-timing-postflash-20261019.log
    python .local/maps/measure_preview_com17_20261019.py
    python .local/maps/measure_preview_buttons_com17_20261019.py

Postflash control checks PASS. The first canvas-tap phase acknowledged all 100
commands but did not advance camera generation and produced no preview samples;
it is invalid as a pan measurement and is excluded. Twelve alternating keyboard
zoom commands yielded three scaled samples. The explicit pan-button follow-up
acknowledged 24/24 commands, advanced generation 6→24 and added 18 same-scale
previews. Cumulative same-scale statistics include one earlier zoom-return sample.

| Preview metric | Cumulative samples | Median | p95 / max |
| --- | ---: | ---: | ---: |
| Same-scale paint | 19 | 116.189 ms | 121.629 ms |
| Same-scale request→paint completion | 19 | 116.434 ms | 121.881 ms |
| Scaled paint | 3 | 246.232 ms | 251.991 ms |
| Scaled request→paint completion | 3 | 252.251 ms | 272.561 ms |

Completed frame pushes were about 77.8–79.0 ms in these observations. Three
scaled samples are insufficient for broad percentile claims. Rolling presentation
intervals include inter-run dwell (including large pauses), so their p95/max are
not continuous visible FPS. The software starts at accepted draw requests, not
physical touch sampling or host command transmission; panel scanout is unmeasured.
No physical touch-latency or 33 ms success claim follows. Paint alone exceeds
33 ms here; clear/transfer separation is the next profiling requirement.

Both movement runs retained approximately 2.4 MSPS with zero driver drops/overruns
before/after and unchanged frequency. Logs/results:

- preview-timing-camera-20261019.log and preview-timing-camera-results-20261019.json
- preview-timing-buttons-20261019.log and preview-timing-buttons-results-20261019.json
- preview-timing-camera-run-20261019.log and preview-timing-buttons-fixed-run-20261019.log

The first button-helper launch had a local relative-path error before device
access. It was corrected using absolute paths; failed launch output is retained.

## Retained-map SD and navigation

    python .local/maps/com17_label_bench.py preview-timing-sd-plan-20261019.json preview-timing-sd-20261019.log

SD-specific checks PASS: uniquely named 8 KiB scratch file uploaded/read back,
matching SHA-256 dc404a613fedaeb54034514bc6505f56b933caa5250299ba7d094377a51caa46.
Map holder stayed 2,835,456 bytes before/after. Reception was intentionally paused
for bulk SD transfer and resumed afterward. Existing files were not overwritten;
the new test scratch file remains /orcsdr/maplabels_330606cb_timing_20261019.bin.
Log: preview-timing-sd-20261019.log. Source and readback bytes are under .local/maps/.
The combined script later failed on an expired authentication session at Home;
this was not an SD failure or device crash. No repeat write was issued.

Fresh authentication and navigation recovery:

    python .local/maps/com17_label_bench.py preview-timing-navigation-plan-20261019.json preview-timing-navigation-20261019.log

PASS: Home→RF Lab→Home and repeated Settings→Home returned with graphics=1 in
three snapshots. Map reopened and reached ready; driver STREAMING near 2.4 MSPS,
zero drops/overruns, C6 still matching 3.0.6. This checks routing/state, not visual
pixel overlap. User subsequently confirmed: "Looks correct; dragging works" for the controls,
city-name behavior and dragging without blackout on this deployed candidate.
This physical confirmation applies to the preview-timing candidate, not the
subsequent exposed-strip patch.

Latest captured map-open memory: DMA free 10,283 / largest 9,728 / minimum 7,631;
internal free 15,983 / minimum 7,751; PSRAM free 19,143,156 bytes. Main stack
high-water mark 3,692. These workload-specific values do not establish a safe
global memory budget or pairing acceptance. COM17 is left receiving on the map
picker; serial control/monitor ports are released.

## Remaining work

Optimize the measured preview costs while preserving display ownership, clipping
and the required orientation. No direct-DMA shortcut is authorized by these
results. Native-tested engine 722d0d3 still needs real-global-pack installation
and worldwide acceptance. Boundary candidate 3d9d01c passed its separate 7/7
host run but is not deployed. Setup/download, global quality, final memory and
radio/SD matrix, review and OrcSDR main merge gates remain open.
