# Exposed-strip preview validation — COM17, October 19, 2026

Scope: one approved native build, COM17 P4 app-only update and bounded pan/zoom,
retained-map SD, memory, navigation and radio checks. COM39 belongs to Claude's
Dead Air work and is untouched. No C6 firmware update, NVS/SD/world erase,
map-data publication or engine pin update is included.

## Build and inspection

Candidate implements `4eaf9fb5`, built at source `eb04ae2e0cbd712f6918e82121553def44ddd4fd` with
preserved dirty/untracked wizard changes. Their hashes and tracked diff hash
are sealed in `.local/maps/preview-strip-4eaf9fb5-20261019/manifest.json`.
New location-save acknowledgement/reporting changes requested during the build
are separate source work and are not in this sealed candidate.

Invoked from the integration worktree root:

    C:/Espressif/python_env/idf5.5_py3.14_env/Scripts/python.exe .local/maps/build_preview_strip_20261019.py
    C:/Espressif/python_env/idf5.5_py3.14_env/Scripts/python.exe .local/maps/inspect_preview_strip_20261019.py

The helper runs ESP-IDF 5.5.4 idf.py from apps/orcsdr-tab5, with explicit saved
SDKCONFIG, C6 3.0.6 image, diagnostics OFF and ESP_IDF_VERSION=5.5. One native
build passed, exit 0. Build log: apps/orcsdr-tab5/preview-strip-native-build-20261019.log.
Inspection log: preview-strip-native-inspection-20261019.log.

- App: 5,062,864 bytes; partition headroom 1,228,592 bytes.
- App SHA-256: `5cad67104ca280d7848ce3965da7dfa933485c2db870696954ab283855a87f48`.
- ELF SHA-256: `28bab1fd866d7a3e63b572aa9d5ffe8c3e2a67a72528a2f8ed2de2b70fb14f36`.
- Both preview pools: 10,368 bytes each, external BSS (20,736 total), adding
  8,288 bytes including metadata relative to the previous linked candidate.
- Label pool 15,416 bytes and update recorder 2,072 bytes remain external BSS.
- Configured internal DMA reserve 40,960 bytes, exception reserve 1,024 bytes.
- Requested/locked/fetched OrcMaps `722d0d3` verified; dependency lock unchanged.
- Existing miniz/FT8 symbol gates pass. Correct C6 image embedded; no C6 flash.

## Preflight

    python .local/maps/com17_label_bench.py preview-strip-preflight-plan-20261019.json preview-strip-preflight-20261019.log
    python .local/maps/com17_label_bench.py preview-strip-stop-plan-20261019.json preview-strip-stop-20261019.log

PASS: USB serial 30:ED:A0:E2:E6:95, Blog V4 driver 0.9.3 streaming near
2.4 MSPS, drops/overruns zero, BROWSE 26,591,754 Hz; C6 3.0.6 match=1.
Reception stopped successfully for flash. Existing automation key used without
pairing/replacement. User confirmed the previous preview-timing candidate's
controls/city-name behavior and dragging: “Looks correct; dragging works”.
That physical acceptance does not apply automatically to this new candidate.

## Flash, startup and readiness

    python .local/maps/flash_preview_strip_20261019.py

PASS: previous deployed app ae0d94b1 verified; fresh NVS/phy/coredump/world
backups read and verified before the app-only write at 0x10000. Candidate,
bootloader, partition table and all non-app partitions verified afterward.
No rollback required; previous app and full recovery retained.
Logs: preview-strip-flash-run-20261019.log and individual
preview-strip-{verify-previous,backup-*,verify-backups,app-write,preservation}-20261019.log.

Native observation from apps/orcsdr-tab5:

    idf.py -B build-native-hosted3 -p COM17 monitor --no-reset

One explicit monitor Ctrl+T Ctrl+R booted the app from the approved flash;
Ctrl+] released the port afterward. Log:
apps/orcsdr-tab5/preview-strip-boot-native-monitor-20261019.log.
Startup passed: SD/Wi-Fi/RTL ready. C6 SDIO initialization transiently retried,
then succeeded; Wi-Fi retried and connected. These are recorded observations,
not attributed to the map change. The preserved old 6,080-byte coredump is not
a newly observed crash. No new panic/reset observed during bounded tests.

The first postflash script checked STREAMING while the driver was STARTING;
RTL_START ESP_OK followed 315 ms later. That script failed its premature
assertion; no rebuild/reset/flash followed. Fresh authenticated readiness,
frequency restoration and map opening then passed:

    python .local/maps/com17_label_bench.py preview-strip-postflash-ready-plan-20261019.json preview-strip-postflash-ready-20261019.log

Original BROWSE 26,591,754 Hz restored. Two frames retained 2,835,456 bytes.
Logs preserve both the initial failure and successful followup.

## Bounded camera measurements

    python .local/maps/measure_preview_strip_buttons_com17_20261019.py
    python .local/maps/measure_preview_strip_zoom_com17_20261019.py

PASS: 24/24 pan button commands and 12/12 zoom key commands acknowledged.
The pan-only phase produced 22 same-scale preview samples; the zoom phase
produced three scaled samples and one additional same-scale return sample.
The table below uses the pan-only snapshot, not the later cumulative bucket.

| Path / metric | Samples | p50 ms | p95 ms | max ms |
| --- | ---: | ---: | ---: | ---: |
| Pan paint_call | 22 | 69.116 | 98.516 | 99.473 |
| Pan request_feedback | 22 | 69.486 | 98.794 | 99.738 |
| Pan clear_call | 22 | 0.015 | 17.585 | 17.661 |
| Pan transfer_call | 22 | 57.272 | 98.493 | 99.448 |
| Zoom paint_call | 3 | 247.549 | 252.902 | 252.902 |
| Zoom request_feedback | 3 | 253.189 | 253.724 | 253.724 |
| Zoom clear_call | 3 | 32.573 | 32.918 | 32.918 |
| Zoom transfer_call | 3 | 214.611 | 220.312 | 220.312 |

**33 ms is not met.** Transfer/scaling remains the dominant measured cost.
The previous candidate's pan p50/p95 was 116/122 ms; differences in retained
source/view, sample mix and scheduling prevent treating these as a controlled
percentage speedup. Only three zoom samples do not establish general p95.
Independent stage percentiles must not be added as though they describe one
sample. Raw presentation intervals include inter-run dwell and scheduled input
spacing; no continuous-FPS, physical-touch latency or panel-scanout claim.
One preview lock miss was reported after zoom; no pixel-correctness conclusion
follows from timing counters. Completed canvas pushes remained around 78–80 ms.

Radio stayed STREAMING near 2.4 MSPS before/after each workload, zero observed
driver drops/overruns, C6 3.0.6 match. Antenna suitability was not independently
verified; no decoding-quality claim. Camera logs/results at worktree root:
preview-strip-buttons-20261019.log, preview-strip-buttons-results-20261019.json,
preview-strip-camera-20261019.log, preview-strip-camera-results-20261019.json
(and corresponding *-run-20261019.log).

## Retained-map SD and navigation

    python .local/maps/com17_label_bench.py preview-strip-sd-plan-20261019.json preview-strip-sd-20261019.log
    python .local/maps/com17_label_bench.py preview-strip-navigation-plan-20261019.json preview-strip-navigation-20261019.log

Both complete scripts PASS. Per-step authentication avoids the previous expired
session. Reception paused for bulk SD transfer and resumed afterward. Unique
8 KiB scratch write/readback hash matched:
`dc404a613fedaeb54034514bc6505f56b933caa5250299ba7d094377a51caa46`.
Map holder stayed 2,835,456 bytes. Existing files were not overwritten;
new scratch remains `/orcsdr/maplabels_330606cb_strips_20261019.bin`.
Local source/readback .local/maps/preview-strip-sd-{scratch,readback}-20261019.bin.

Home→RF Lab→Home and repeated Settings→Home passed with graphics=1 in three
state snapshots; map reopened ready. Driver STREAMING, drops/overruns zero;
no physical absence-of-overlap claim. Device left receiving on map picker,
serial port released. Latest health snapshot:

    RTL_HEALTH_STATUS uptime_ms=200717 free_heap=19139392 min_free_heap=18738784 dma_free=10151 dma_min=7435 dma_largest=9728 tasks=31 main_stack_hwm=3692 reset_reason=11 internal_free=15947 internal_min=7587 internal_largest=9728 psram_free=19133584 psram_largest=18874368

Memory values are workload-specific, not a closed pairing/headroom gate.
The user subsequently confirmed physical acceptance: “Dragging/zoom and
controls look good”. Location-save feedback
fix is source-only and requires a separately approved native cycle. Remaining
map speed, global pack/quality, delivery/setup and final integration gates stay
open; no new overnight soak requirement.
