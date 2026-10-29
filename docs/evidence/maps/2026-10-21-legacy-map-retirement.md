# Retire personal regional-map defaults and legacy rendering

The user identified a Lane County/Springfield-looking map flashing during ADS-B
zoom and clarified the shipping requirement: OrcSDR must be a worldwide product,
without the developer's personal regional setup as its default experience.
This supersedes the earlier requirement to retain the ORCMAP1 renderer.
Existing user files and saved receiver settings must still be preserved.

## Confirmed cause and source changes

`map_view.cpp` previously rendered ORCMAP1 whenever the asynchronous OrcMaps
frame was pending. This overwrote the shared cached preview and temporarily
changed both labels and overlay projection. It also explicitly bypassed OrcMaps
when the saved pack identity was `lane_county_map`.

- Remove `offline_map.cpp/.hpp`, its CMake source entry, startup/catalog reload
  calls and its boot self-check dependency.
- Regional dashboards always use the OrcMaps camera/service. Pending frames
  use its transformed cached preview, including the preview's original credit.
  No ORCMAP1 draw or projection path remains.
- Store preview metadata in PSRAM, under the same publication lock as its
  pixels and camera. No added internal/DMA buffer is required.
- Retain an existing old file only as an unsupported, unselectable inventory
  entry. Do not manufacture that entry on a clean device. Do not delete files.
- Retire the historical regional-map slot from public data-catalog state,
  serial help and install/remove operations. Preserve internal signed-catalog
  indices/parsing so older signed catalogs and other data packs still work.
- Replace the receiver-location boot self-check's personal place strings and
  coordinates with neutral test values. The self-check restores saved state.

This does not remove user-installed regional PMTiles packs. Such packs are valid
OrcMaps data; switching to them must follow coverage/selection policy, never
temporary legacy fallback. This does not claim an application-wide product
defaults audit. At this checkpoint UI-document demo content still included an
Oregon example, separate from first-boot state. The subsequent
[source-camera checkpoint](2026-10-21-pack-source-camera.md) neutralizes that
demo and validates its combined native build on COM17.

## Focused regression

Invocation directory: `F:/AI/OrcSDR-TEMP/maps-integration-completion`.
Linux commands run through WSL:

```text
cmake -S apps/orcsdr-tab5/tests/maps -B .local/maps/labels-host
cmake --build .local/maps/labels-host --target orcsdr_map_view_tests orcsdr_map_preview_tests orcsdr_maps_tests -j2
.local/maps/labels-host/orcsdr_map_view_tests
.local/maps/labels-host/orcsdr_map_preview_tests
.local/maps/labels-host/orcsdr_maps_tests /mnt/f/AI/OrcSDR-TEMP/orcmaps-quality-delivery/tests/fixtures/tiny-gzip.pmtiles /mnt/f/AI/OrcSDR-TEMP/orcmaps-quality-delivery/examples/m5stack-tab5/test-pack/springfield-97477.manifest.json .local/maps/fit-order-b59d478-20261020/orcmaps_world-before.bin
```

Before correction, the new production-adapter test failed to link because it
still required the legacy renderer's availability, drawing and projection
functions (`map-view-legacy-link-before-20261021.log`). After removal it links
without that renderer and PASSes pending/ready/unavailable cases, stale legacy
identity routing, overlay origin and pending attribution. Existing preview and
actual embedded-world/pipeline/screen checks also pass.
Logs: `map-view-orcmaps-only-*-20261021.log`,
`map-view-preview-test-20261021.log`, `map-view-world-test-20261021.log`.
The rerun with the final neutral self-check is recorded in
`map-view-retirement-{host-build,host-test,world-test}-20261021.log`.

Native build, binary symbol removal, COM17 flash/preservation, serial catalog
behavior and physical zoom acceptance were pending at this source checkpoint.
The continuation below records later gates. COM39 is untouched.

## COM17 continuation — source `1308e5bd`

Native build and artifact inspection PASS:

```text
python .local/maps/build_fit_order_20261020.py build orcmaps-only-20261021
C:/Espressif/python_env/idf5.5_py3.14_env/Scripts/python.exe .local/maps/inspect_orcmaps_only_20261021.py
```

Logs in the invocation root: `fit-order-native-orcmaps-only-20261021-20261020.log`
and `orcmaps-only-native-inspection-20261021.log`. Inspection confirms **no
`orcsdr::offline_map::` symbols**, unchanged 40 KiB DMA reserve, unchanged engine
pin and checked PSRAM placements, including the new preview metadata.
App: 5,126,768 bytes, 4,208 fewer than the prior app; factory headroom 1,164,688
bytes. SHA-256: `1ec2ebbafa5197517004c565ce6024d38afc18cc221f38acd5a97681e59cd3ad`.
ELF SHA-256: `f29ca9a830e48c9ba95daa77d644017fed56cc1681b618e6c4d147dc4caa9156`.

App-only flash on COM17, USB serial 30:ED:A0:E2:E6:95:

```text
python .local/maps/com17_label_bench.py logical-label-preflash-plan-20261021.json orcmaps-only-preflash-20261021.log
python .local/maps/flash_orcmaps_only_20261021.py
```

Previous app, fresh non-app backups, write and post-write preservation verification
all PASS. Logs: `orcmaps-only-{verify-previous,backup-*,verify-backups,app-write,preservation}-20261021.log`.
Artifacts/backups: `.local/maps/orcmaps-only-1308e5bd-20261021/`.
Boot, partition table, NVS, world and old core dump remain intact; SD files and
C6 firmware are untouched. Rollback app is the saved `b694208a` candidate.

Native monitor used `idf.py -B build-native-hosted3 -p COM17 monitor --no-reset`,
then one explicit Ctrl-T/Ctrl-R reset. Log in its invocation directory:
`apps/orcsdr-tab5/orcmaps-only-boot-native-monitor-20261021.log`.
Boot reaches SD/Wi-Fi/RTL ready; ELF hash matches. The preserved core dump is
historical, not a new crash.

```text
python .local/maps/com17_label_bench.py orcmaps-only-device-plan-20261021.json orcmaps-only-device-20261021.log
python .local/maps/com17_label_bench.py orcmaps-only-zoom-plan-20261021.json orcmaps-only-zoom-20261021.log
```

Both return exit 0. Public catalog lists the four supported historical data
entries and no regional map. `RTL_CATALOG_INSTALL lane_county_map` returns
`unknown_or_unchecked_pack`, without a download or file removal.
The initial navigation attempt did not produce zoom generation changes, so it
is not zoom acceptance. The subsequent explicit Home→ADS-B→Radar sequence checks
screen ownership, changes range 25→10→25, and produces two scaled-preview samples
and ready generation 3 with zero missing/failed tiles. Sample paint calls are
9,954 and 23,978 us; **two samples do not close the overall 33 ms target**.
Physical no-flash acceptance PASS: on October 21, the user confirmed on COM17,
"Old map flash is gone." This closes the reported legacy-map zoom flash for
installed source `1308e5bd`; it does not close other map-quality gates.

Bounded health snapshot: free PSRAM 20,697,576 bytes, largest DMA block 9,216
bytes. Radio was streaming; this is not a controlled continuity/queue-loss soak.
Next milestone: regional/global pack coverage and
the broader product-default audit (including remaining documentation demos).
