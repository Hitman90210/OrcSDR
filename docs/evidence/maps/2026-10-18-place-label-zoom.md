# Place names at regional zoom

User requested a clear default world map, with city names appearing only closer in.
The shared OrcSDR pipeline now excludes place-label layers below zoom 6. At
zoom 6 and above, existing per-feature visibility, font and collision rules apply.
This affects place names, including town names, across shared map consumers.
No engine dependency or map data changes are needed.

Changed source: `apps/orcsdr-tab5/ui/map_pipeline.cpp` and
`apps/orcsdr-tab5/tests/maps/map_pipeline_tests.cpp`. Uncommitted at this checkpoint.

Focused host command (WSL, from integration worktree root):

```sh
cmake --build .local/maps/preview-pinned --target orcsdr_maps_tests
ORCSDR_LABEL_TEST_ARCHIVE=/mnt/f/AI/OrcSDR-TEMP/orcmaps-quality-delivery/.local/global-assessment/global-z8-candidate.pmtiles ctest --test-dir .local/maps/preview-pinned -R '^map_pipeline$' --output-on-failure
```

Result: 1/1 passed. Real global archive views at zoom 3 and 5 offer/draw no
place names; existing zoom-8 checks still find Eugene and London and verify
deterministic repeated output and absence of stale names after moving.
Logs: `city-label-build-20261018.log`, `city-label-test-20261018.log` in the
integration worktree root.

No native build or hardware used. COM17 and COM39 remain unchanged.
Next gate: approved native build and COM17 visual validation of the transition.

Follow-up transition regression: zoom 6 offers/draws names, then returning to
zoom 3 clears them. The focused command above passed again, 1/1, with these
additional assertions. Logs: `city-label-transition-build-20261018.log` and
`city-label-transition-test-20261018.log`. This remains host evidence only.

## Approved native attempt — artifact rejected before flash

User approved one native build and COM17 app-only checks. Read-only COM17
preflight passed: identity 30:ED:A0:E2:E6:95, C6 3.0.6, driver 0.9.3 streaming
with zero reported drops/overruns. Logs: `city-label-preflight-20261018.log`
and `city-label-preflight-run-20261018.log` in the worktree root.

The initial invocation failed before compilation because its build ID exceeded
39 characters (`apps/orcsdr-tab5/city-label-native-build-20261018.log`). A shorter
ID compiled successfully; miniz and FT8 gates passed. However, the regenerated
cache lacked the explicit C6 firmware input and prior SDKCONFIG path. The app
was unexpectedly smaller, `0x3be050` bytes. It was rejected and **not flashed**.
Compiler success is not approval of this changed configuration.

Build log: `apps/orcsdr-tab5/city-label-native-build-short-id-20261018.log`.
Inspection log: `apps/orcsdr-tab5/city-label-native-inspection-20261018.log`.
The inspection script also rejected disabled diagnostic spelling `OFF` versus
`0`; both mean disabled, so that assertion alone is not a firmware defect.
The missing C6 input is the substantive configuration mismatch.

Corrected native build needs explicit saved `build-native-hosted3/sdkconfig`,
C6 3.0.6 image input and startup diagnostics disabled. It requires renewed
approval under the user's repeated-build rule. COM17 and COM39 remain unchanged;
the saved preview app remains the immediate rollback candidate.

Separate preview-duration instrumentation passed its focused host timing test
(1/1). It was saved outside the build source in
`.local/maps/preview-timing-prepared-20261018/` and excluded from this candidate.
It is not covered by the city-label device approval and is not device-tested.

## Corrected build and COM17 bounded checks

User approved the corrected cycle. Explicit SDKCONFIG/C6 inputs retained;
pin, miniz and FT8 gates pass. Inspection verifies disabled diagnostics,
40,960-byte DMA reserve, label pool in external BSS (15,416 bytes), and exact
expected C6 image bytes embedded in the app. App 5,059,536 bytes, SHA-256
`a78b6f2400bd62b22e0baccf06cf2802cbb8788897540ccc97600089f2e4689c`.
Build identity: `maps labels 477f087a dirty 2026-10-18`; source includes preserved
wizard/gesture/blackout edits and the label policy, excludes new timing work.
No source commit has yet been made for the label policy.

Commands executed from `apps/orcsdr-tab5`: configured `idf.py -B
build-native-hosted3` with explicit `SDKCONFIG=build-native-hosted3/sdkconfig`,
saved C6 3.0.6 image input, `ORCSDR_MAP_STARTUP_DIAG=OFF`, then `build`;
`python city_label_native_inspect_20261018.py` passed. Logs:
`city-label-native-build-explicit-20261018.log` and
`city-label-native-inspection-explicit-20261018.log` in that directory.

From integration root, Python `.local/maps/flash_city_labels_20261018.py`
verified COM17 USB/ROM identity, previous preview app and fresh NVS backup;
wrote only P4 app 0x10000; preservation verification passed for bootloader,
table and saved non-app partitions. Recovery/artifacts:
`.local/maps/city-labels-477f087a-20261018/`. Prior preview candidate remains
rollback. No COM39, C6 or SD pack changes. Flash logs use `city-label-` prefix
in root, including `city-label-flash-run-20261018.log` and
`city-label-preservation-20261018.log`.

`python .local/maps/capture_com17_boot.py city-label-boot-20261018.log`
captured startup without a panic. Then `python .local/maps/com17_label_bench.py
city-label-device-plan-20261018.json city-label-device-20261018.log` passed the
bounded pan/zoom plan while receiving at 14.070 MHz. Seven status captures:
ready frames, zero offered/drawn overview labels. Final driver approximately
2.393 MSPS, drops=0/overruns=0; C6 host/coprocessor both 3.0.6, matching.
Map holder 2,835,456 bytes. DMA free 10,395, minimum 7,547, largest 9,728;
internal free 16,087/minimum 7,691; PSRAM free 19,042,468. These measurements
do not close memory-headroom acceptance or establish an improvement.

Device left on picker receiving; serial port released. User physically confirmed
**"World view clear; controls and dragging good"**. This accepts the opening
view and preserved controls/cached dragging on this COM17 candidate.
Zoom-6 label appearance is host-verified, not established
by this device run on installed packs. Continuous drag timing remains open.
