# LoRa map flicker and approved clustered-name layout

The user reports visible LoRa-map flicker after the row-batching experiment.
That experiment is retired and COM17 restored to `9ab05c04` while these source
corrections are prepared. Hardware rollback preserves NVS/SD/world/C6 and boots
the matching saved ELF. The defect is not attributed solely to the experiment.

Two independent redraw causes are reproduced in focused tests:

- `map_view.cpp` clears the entire map before calling the shared service, even
  when an opaque completed frame or cached preview immediately overwrites it.
  Pending without metadata can mean a preview-publication lock miss; it must
  preserve the visible image too. Only explicit unavailable/no-credit fallback
  clears the empty map area. Attribution/status behavior remains shared.
- `lora_dashboard::update()` redraws the map and selected-node panel for age ticks
  or unrelated radio counters. Map updates now depend on node identity, names,
  positions/selection or a new shared map revision. Header state updates remain
  independent. Camera actions and asynchronous completed frames still redraw.

The approved bounded node-name layout keeps every positioned marker, paints the
selected/followed name first, and tries ten nearby size-2 slots for other names.
It avoids accepted names, other markers, zoom controls and the map/credit bounds.
Names that cannot fit remain in Nodes; no nodes or saved data are deleted. It
uses a fixed 16-entry PSRAM record array, no heap allocation or new framebuffer.
UI and existing named serial camera controls share the same draw path.

Tests first fail on the original full-map clear and idle redraw
(`lora-map-flicker-red-20261022.log`, 0/2); the clustered-label regression then
fails overlapping names (`lora-cluster-label-red-20261022.log`). Corrections pass
the focused map-view/LoRa checks and sanitizer checks. Tests cover initial/pending/
ready/unavailable frames, telemetry ticks, publication, ownership, camera motion,
Follow identity and crowded-name priority/bounds/separation. Full-app flicker
acceptance is not inferred from these bounded checks.

Commands from the integration root:

```text
wsl -d Ubuntu -- bash -lc 'cd /mnt/f/AI/OrcSDR-TEMP/maps-integration-completion && cmake --build .local/maps/labels-host --target orcsdr_maps_tests orcsdr_map_view_tests orcsdr_lora_map_tests orcsdr_adsb_map_range_tests -j2 && ctest --test-dir .local/maps/labels-host -R "^(map_pipeline|map_view|lora_map_controls|adsb_map_range)$" --output-on-failure'
wsl -d Ubuntu -- bash -lc 'cd /mnt/f/AI/OrcSDR-TEMP/maps-integration-completion && cmake --build .local/maps/map-serial-asan-20261021 --target orcsdr_map_view_tests orcsdr_lora_map_tests -j2 && ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir .local/maps/map-serial-asan-20261021 -R "^(map_view|lora_map_controls)$" --output-on-failure'
python .local/maps/check_lora_map_syntax_20261021.py
```

Final logs `lora-flicker-final-{host,asan}-20261022.log` and
`lora-cluster-flicker-p4-20261022.log`. Native installation, physical flicker/name
acceptance and representative performance remain open. COM39 untouched.

## COM17 candidate installed

Source `51318792` builds and passes artifact inspection: app 5,159,568 bytes,
headroom 1,131,888; app SHA-256
`008be0908286a2db2d38fe488a36e26acfd4708233a0b826f08b700eb0a322a2`, ELF
`4ade01d5733f565bbb38a998d674bb2080934a31eea8e85284b59d56b535577a`.
DMA reserve remains 40 KiB, diagnostics OFF, exact engine pin unchanged, TEST-ONLY
catalog still explicit. ELF places the fixed label records in external BSS at
`0x48527fd8`, size 384 bytes; no `g_preview_stripe` symbols remain.

Commands from the integration root:

```text
python .local/maps/build_pilot_20261022.py build lora-flicker-labels-20261022
python .local/maps/inspect_lora_flicker_labels_20261022.py
python .local/maps/com17_label_bench.py scaled-preview-preflash-plan-20261022.json lora-flicker-labels-preflight-device-20261022.log
python .local/maps/flash_lora_flicker_labels_20261022.py
python .local/maps/com17_label_bench.py scaled-preview-postflash-retry-plan-20261022.json lora-flicker-labels-postflight-after-startup-device-20261022.log
python .local/maps/com17_label_bench.py lora-flicker-labels-idle-capture-plan-20261022.json lora-flicker-labels-idle-capture-device-20261022.log
```

All listed commands exit zero. Current-app verification, fresh backups and
preservation comparisons pass; only P4 app changes. Matching ELF boot reaches
Ready, SD/Wi-Fi/RTL ready, C6 3.0.6 preserved. Boot capture uses the exact ELF and
native monitor; invocation log lives in
`apps/orcsdr-tab5/lora-flicker-labels-boot-native-monitor-20261022.log`.
Remaining logs live beside the invoked scripts in the integration root, prefix
`lora-flicker-labels-`; build log retains wrapper prefix
`fit-order-native-lora-flicker-labels-20261022-20261020.log`.

The first postflight file without `after-startup` fails its active-map assertion:
Open and View acknowledge, but ownership later reports inactive. It occurred
during startup completion; a delayed splash transition is a possible explanation,
not proven solely by the log. This failed run is retained, not counted as success.
The subsequent post-startup run passes. A real background CRC-valid LoRa packet
decodes during observation; no controlled transmitter packet is sent here.

During the 12-second bounded still-view observation, generation/revision remains
2/2 and cached pushes rise from 3 to 4. This verifies that RF/status activity no
longer causes the former periodic continuous redraw, but does not prove zero
repaints or establish physical flicker acceptance. No missing/failed tiles.
Native screenshot shows four separated readable node IDs, intact controls and
attribution; undisplayed crowded names remain in Nodes. The selected node at that
instant lacks a map position, so its sidebar identity remains visible separately.

Screenshot write succeeds while receiving; serial readback pauses reception with
map memory retained, then LoRa resumes. Local file:
`.local/maps/lora-flicker-labels-51318792-20261022/map.bmp`, 2,764,854 bytes,
SHA-256 `ac8eea378b7d705e734819e1752b5f1f20058f40908ee2edc69862a4f0bf7fa2`.
Saved receiver E7 stays `440628866,-1230160273`. Final radio streams about
959 ksps, zero reported driver drops/overruns. Final free PSRAM 2,090,772 bytes,
largest 1,998,848; internal free 16,087, largest DMA 8,192; main stack high-water
margin 1,128; reset reason unchanged. This is a bounded check, not a soak.

Physical acceptance — October 22: the user confirms on COM17, “Flicker stopped;
movement looks good.” This closes the stationary/moving flicker and visible-control
check for installed `51318792`. It does not independently confirm every node-name
readability case or measure physical touch latency. The overall 33 ms interaction
and integration goals remain open.
