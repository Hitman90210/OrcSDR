# LoRa map interaction checkpoint

Source commit: `050442b0` on `codex/maps-integration-completion`.
Remote main remains `fba861e36970357e0a005c03d666250b09096cb6`, published integration
branch `af877ed7189d99f99873f61f5be3995ad893db3b`, cloud FT8 branch
`2e86835bb75720ab3286eac6e95c40d50e9b7c99`. No push or merge performed.

## Behavior implemented

- LoRa owns one OrcMaps camera, rendered through the existing shared asynchronous
  map service and cached-frame preview. No second basemap engine or worker.
- Initial center prefers the configured receiver, then the selected positioned
  node. Receiver and Node Center are separate controls. Follow tracks that selected
  node, not the first other node with a position. Manual pan disables follow.
- Continuous drag starts after a six-pixel movement threshold. The initial touch
  does not recenter. Zoom retains the center, bounded to 0–16. The initial scale
  remains 15 NM; overzoom adds no source detail.
- Node and receiver markers project through the returned basemap camera. Marker
  drawing is clipped within the map, leaving the attribution strip intact.
  Zoom controls are drawn after markers. Bottom controls now end above the tabs.
- Completed map publications repaint from the UI loop even without fresh radio
  snapshots. Pan skips unchanged sidebar/toolbar repaint; source decoding stays
  on the shared worker. Hardware frame time remains unmeasured.
- Named authenticated `RTL_LORA_MAP` actions call the same control handler as
  touch. Read-only Help/Status report ownership, readiness, center, zoom and follow.
  Controls reject inactive/wrong-owner views, malformed/oversized commands,
  out-of-range/zero pan and unmappable selected positions. No receiver NVS write.
- Valid polar node coordinates remain node data; this regional Mercator map cannot
  display them above about ±85.05°. Missing mappable positions show an explicit
  message; no personal/default location is invented.

Changed files: LoRa dashboard `.cpp/.hpp`, the shared `map_view.cpp/.hpp` camera
entry point, narrow main command/touch/help registrations, map test CMake and
adapter test, `tests/lora_map_controls_tests.cpp`, and the existing FT8 recording
display stub. The native LoRa camera is 32 bytes in ordinary data memory
(`4ff437f8`); no new full-size framebuffer or permanent DMA pool was added.

## Tests and actual results

Invocation directory: `F:/AI/OrcSDR-TEMP/maps-integration-completion`.
WSL equivalent: `/mnt/f/AI/OrcSDR-TEMP/maps-integration-completion`.

```text
cmake -S apps/orcsdr-tab5/tests/maps -B .local/maps/labels-host
cmake --build .local/maps/labels-host --target orcsdr_lora_map_tests orcsdr_map_view_tests -j2
.local/maps/labels-host/orcsdr_lora_map_tests
.local/maps/labels-host/orcsdr_map_view_tests
cmake -S apps/orcsdr-tab5/tests/maps -B .local/maps/map-serial-asan-20261021
cmake --build .local/maps/map-serial-asan-20261021 --target orcsdr_lora_map_tests orcsdr_map_view_tests -j2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 .local/maps/map-serial-asan-20261021/orcsdr_lora_map_tests
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 .local/maps/map-serial-asan-20261021/orcsdr_map_view_tests
```

All four final executions PASS. The sanitizer cache is Debug with
`-fsanitize=address,undefined -fno-omit-frame-pointer`; the ordinary host cache
has no build-type/optimization override. No host timing claim is made.

The LoRa test uses the real dashboard, command parser, gesture handler, shared
map-view adapter and OrcMaps camera/projection functions. The recording display,
location/preferences, async worker and initial regional-scale helper are fakes;
this is not device pixel or initial-scale acceptance. Cases include auth and
ownership, strict parsing and bounded pan, receiver/selected-node center, later
follow positions, drag/serial equivalence without touch-down jump, antimeridian
wrap, zoom limits, matching marker coordinates, explicit polar rejection and
completed-frame refresh without a radio snapshot.

Initial failed runs are preserved. The recording stub first lacked `rgb565_t`;
one assertion confused the sidebar's node name with the map marker; a source
edited during compilation required rebuilding its stale object. The FT8
regression exposed a test-only `strlcpy` collision with recent glibc, corrected
by using libc where available. Final source was recompiled and rerun after these
corrections. No firmware behavior was weakened to satisfy the assertions.

The existing FT8 dashboard regression was compiled with `g++ -std=c++17 -O2`,
UI include paths, and `tests/ft8_dashboard_tests.cpp` plus `ft8_dashboard.cpp`,
`ft8_model.cpp`, `ft8_conditions.cpp`, `ft8_hunter.cpp`, `ft8_tuning.cpp`,
`ft8_info_text.cpp`, `focus_nav.cpp`. The resulting
`.local/maps/ft8-dashboard-lora-stub-regression-20261021` exits 0 with
`ft8_dashboard_tests: PASS`. This checks the changed recording stub; it does
not close native FT8 decoding/projection acceptance.

Native commands:

```text
python .local/maps/check_lora_map_syntax_20261021.py
python .local/maps/build_fit_order_20261020.py build lora-map-controls-20261021
C:/Espressif/python_env/idf5.5_py3.14_env/Scripts/python.exe .local/maps/inspect_lora_map_controls_20261021.py
```

P4 syntax checks pass for LoRa, shared map view and main. Native build exits 0;
inspection passes the pinned engine, symbol and PSRAM gates, diagnostics OFF,
existing C6 image input and unchanged 40 KiB DMA reserve. App: **5,139,024 bytes**,
factory headroom **1,152,432 bytes**.
App SHA-256: `43f431100b0e44c80b4e9b108f1543956e27e7ef5a04a74e7c8ae52428776565`.
ELF SHA-256: `ab18583d9848be536da9c1cf23d17210aee641800c0a424d3caa58e9ff224a32`.
Artifact: `.local/maps/lora-map-controls-050442b0-20261021/`.

## Saved logs

- `lora-map-host-configure-20261021.log`
- `lora-map-host-build-20261021.log`, `lora-map-host-build-corrected-20261021.log`,
  `lora-map-host-build-final-20261021.log`, `lora-map-host-build-refresh-20261021.log`
- `lora-map-host-build-accepted-20261021.log`, `lora-map-host-test-accepted-20261021.log`
- `lora-map-view-host-test-20261021.log`
- `lora-map-sanitizer-configure-20261021.log`, `lora-map-sanitizer-build-final-20261021.log`,
  `lora-map-sanitizer-test-final-20261021.log`, `lora-map-view-sanitizer-test-final-20261021.log`
- `lora-map-ft8-dashboard-build-20261021.log`,
  `lora-map-ft8-dashboard-build-corrected-20261021.log`, `lora-map-ft8-dashboard-test-20261021.log`
- `lora-map-p4-syntax-accepted-20261021.log`
- `fit-order-native-lora-map-controls-20261021-20261020.log`
- `lora-map-native-inspection-20261021.log`, `lora-map-camera-placement-20261021.log`

## Hardware and remaining milestone

No device was accessed in this checkpoint. COM17 remains on `ba0e7a92`, pending
release of the serial port; COM39 remains reserved to the other work. This
candidate also contains the uninstalled ADS-B range correction `19856eea`.

Next: one COM17 app-only validation with identity/current-app/recovery checks,
physical and named map gestures, controls/edges/labels, selected-node follow
where real positioned nodes exist, receiver-coordinate preservation, memory,
retained-map SD and radio continuity. Restore the user's prior view/tuning.
Neither smooth physical motion nor 33 ms performance is established by these
host results. Full global-pack/HTTPS delivery, complete setup recovery and the
remaining dashboard/RF/performance gates stay open.
