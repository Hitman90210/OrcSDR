# Maps setup checkpoint — COM17, October 19, 2026

Current-session approval covers routine native build, app-only COM17 flash and
bounded regression checks. COM39 reserved for Claude; C6/NVS/SD/world preserved.

## Source and build

Checkpoint210002f4 includes the reviewed preserved wizard/location work, shared
editor generation ownership, one-time external-exit teardown and saved-label/
SAVING feedback. Parentf38d1f78 adds read-only panel/clip/depth/copy-area facts.
Source tracked state is clean except documentation changes. No managed edits.
This checkpoint does not establish completed downloads or first-boot setup.

Invoked from integration root:

    python .local/maps/build_maps_checkpoint_20261019.py
    python .local/maps/inspect_maps_checkpoint_20261019.py

One native build PASS, ESP-IDF5.5.4 explicit saved SDKCONFIG, C63.0.6 input,
map/Wi-Fi diagnosticsOFF. Requested/locked/fetched engine722d0d3 unchanged;
miniz/FT8 gates pass, DMA reserve40960, exception reserve1024. App5,065,024
bytes, headroom1,226,432. Panel facts176bytes externalBSS;
labels15416/update2072/preview20736bytes externalBSS.
App SHA256 `3b4aae6d556f09c6f46c3e57c73f0090c815a6eaabac1af2104860720a58c424`.
ELF SHA256 `4e69c07eb3535daa4d98a27b129337afcad0e0c1273209163575ef1f6e6ebb8d`.
Artifact `.local/maps/maps-checkpoint-210002f4-20261019/` with manifest/BIN/ELF.
Logs apps/orcsdr-tab5/maps-checkpoint-native-build-20261019.log and
maps-checkpoint-native-inspection-20261019.log.

## Hardware checks

    python .local/maps/flash_maps_checkpoint_20261019.py

PASS: COM17 USBserial30:ED:A0:E2:E6:95 verified before each operation, previous
app568acd61 verified; fresh NVS/phy/coredump/world backups and verification.
App-only write0x10000 and post-write candidate/boot/table/non-app preservation
PASS. Logs maps-checkpoint-flash-run-20261019.log and per-operation
maps-checkpoint-{verify-previous,backup-*,verify-backups,app-write,preservation}-20261019.log.

Native monitor from apps/orcsdr-tab5:

    idf.py -B build-native-hosted3 -p COM17 monitor --no-reset

One explicit Ctrl+T Ctrl+R booted flashed app; Ctrl+] released port.
Startup SD/Wi-Fi/RTL ready, C6match3.0.6. No new panic or reset during bounded
checks; preserved existing coredump is historical. Log:
apps/orcsdr-tab5/maps-checkpoint-boot-native-monitor-20261019.log.

    python .local/maps/com17_label_bench.py maps-checkpoint-postflash-ready-plan-20261019.json maps-checkpoint-postflash-ready-20261019.log
    python .local/maps/com17_label_bench.py maps-checkpoint-external-editor-plan-20261019.json maps-checkpoint-external-editor-20261019.log
    python .local/maps/com17_label_bench.py maps-checkpoint-navigation-plan-20261019.json maps-checkpoint-navigation-20261019.log

All three plans PASS. External exit used actual owned setup address input;
receiver IDLE during setup, keyboard editor1 before external Home, editor0
afterward, Home graphics1 and receiver STREAMING afterward. It did not submit
an address or Wi-Fi password. Host tests separately cover password clearing and
not closing a newer unrelated editor. Navigation Home/RFLab/repeatedSettings
returned with graphics1; picker reopened ready. No visual overlap assertion.

## Actual transfer facts / timing

    python .local/maps/measure_maps_checkpoint_buttons_com17_20261019.py
    python .local/maps/measure_maps_checkpoint_zoom_com17_20261019.py

Both bounded workloads PASS. Pan24/24 commands acknowledged,24 samples:
paint p50/p95/max68.716/100.098/100.685ms; clear0.012/16.238/16.267ms;
transfer56.945/100.072/100.664ms. Zoom12/12 commands acknowledged but only two
scaled previews sampled: paint249.535/257.435ms; transfer217.982/233.914ms.
Too few zoom samples for a general percentile claim. Later cumulative pan
bucket26 samples includes zoom-return; use the pan-only snapshot above.
Stage percentiles cannot be added as a single sample. Raw presentation intervals
contain dwell/input spacing; no continuousFPS/physicaltouch/scanout claim.
**33ms is not met.** Zero observed driver drops/overruns near2.4MSPS before/after.

Actual ORC_MAP_PREVIEW_FACTS for physicaldisplay:

    rotation_raw=3 rotation_offset=0 derived_internal_rotation=3
    physical=720x1280 logical=1280x720 source=1248x568
    source_depth=16 target_depth=272 clip=0,0,1280,720
    clipped_map_pixels=708864 copy_bbox_pixels=708864

Copy bounding box exact1 for same-scale, exact0 for scaled geometric estimate.
Facts describe last observed paint, not a percentile-associated sample. Checked
M5GFX enum.hpp453–477 identifies16 as rgb565_2Byte and272 as rgb565_nonswapped
(16|0x0100). Equal lowbitdepth does not prove no conversion; no runtime
pixelcopy.no_convert or fp_copy is captured. Checked
Panel_FrameBufferBase.cpp374–416 direct rowmemcpy requires internalrotation0,
opaque pixels and no_convert. Internalrotation3 therefore rules out that fast
branch, but these observations do not quantify each contributor or authorize
changing the user's orientation/globalrotation/framebuffer ownership.

Logs/results maps-checkpoint-buttons-20261019.log,
maps-checkpoint-buttons-results-20261019.json, maps-checkpoint-camera-20261019.log,
maps-checkpoint-camera-results-20261019.json (plus corresponding *-run logs).

## Retained-map SD / settled return

    python .local/maps/com17_label_bench.py maps-checkpoint-sd-plan-20261019.json maps-checkpoint-sd-20261019.log

SD-specific checks PASS: unique8192byte scratch write/readback SHA256
`dc404a613fedaeb54034514bc6505f56b933caa5250299ba7d094377a51caa46`.
Map holder2835456bytes before/after. New scratch remains
/orcsdr/maplabels_330606cb_checkpoint_20261019.bin; pre-existing files untouched.
The combined plan later failed expecting immediate RTL_TUNE_OK; log shows
RTL_START ESP_OK and subsequent independent driver snapshot STREAMING. No SD
failure/newcrash inferred. A followup retune acknowledged, but immediate state
snapshot caught STOPPING; this followup also failed its too-early assertion.
Neither failure is recorded as complete-script pass; no SD write repeated.

    python .local/maps/com17_label_bench.py maps-checkpoint-final-status-plan-20261019.json maps-checkpoint-final-status-20261019.log

Settled status PASS: Home graphics1, BROWSE26591754Hz, driverSTREAMING near
2.399MSPS, zero drops/overruns, C6still3.0.6match1. LatestDMAfree/largest/minimum
10155/9216/7439bytes, internalfree/minimum15875/7563bytes,
PSRAMfree19131248bytes, mainstackHWM3612. These workload-specific values do not
close memory/pairing or RF-decoding acceptance. Port released, device left Home
receiving. Original receiver point restored in prior cycle; no location writes
in this checkpoint's tests. NoSD/world/C6 or userdatapartition erase.

## Next integration milestone

Close remaining map transfer and geographic/global-pack acceptance with the
measured rotation/byte-order facts, preserving framebuffer ownership. The
global archive is still only local, boundary candidate is not consumer-pinned,
and trusted delivery/complete setup/end-to-end firstboot/final regression and
OrcSDRmain merge remain open. Physical save-feedback and strip gestures were
accepted on their preceding candidates; no blanket physical acceptance claim
for newly added diagnostics/editor teardown. No new overnightsoak required.
