# Keep installed map coverage at closer camera zoom

## Defect and shared adapter correction

The inventory previously required the camera zoom to be within the archive's
stored zoom range. A global z0–8 pack therefore ceased to be eligible at camera
zoom 9. A preferred regional pack outside the requested bounds also prevented
selection of another installed covering pack. Both could unnecessarily fall
back to the coarse embedded overview.

The correction is in OrcSDR's shared integration adapter, using the existing
OrcMaps resolver and camera APIs without engine/API/pack-format changes:

- Honor a preferred pack only when it covers the requested bounds and has
  suitable source detail. Otherwise consider other installed packs.
- Prefer coverage at the requested source zoom, then the highest stored level
  available below it. Existing OrcMaps priority/tie/coverage rules still apply.
- Adapt source zoom and screen tile size together, keeping the requested
  Mercator projection, center and screen scale exactly unchanged. Do not draw
  tiles with an undocumented mismatch between tile zoom and viewport zoom.
- Preserve logical camera zoom for labels; do not manufacture new map detail.
- Keep the embedded overview's existing one-level-coarser rendering policy.
- Add `ORC_MAP_SOURCE camera_zoom=... source_zoom=... title=...` to
  `RTL_MAP_STATUS`, making the selected detail observable over serial.

The same source checkpoint removes Oregon names/coordinates from documentation
demo state and neutralizes an airport service self-check example. Fresh receiver
state remains unconfigured, with empty location/map labels; the UI-document
demo is explicitly separate. Existing device locations and installed SD data
are not rewritten. The old filename remains recognized only for unsupported-file
reporting, and the retired signed-catalog identifier remains an internal parser
compatibility slot. Neither is a usable renderer or a shipping regional default.

## Tests executed

Invocation directory: `F:/AI/OrcSDR-TEMP/maps-integration-completion`.
Commands below run through WSL:

```text
cmake --build .local/maps/labels-host --target orcsdr_maps_tests -j2
ORCSDR_LABEL_TEST_ARCHIVE=/mnt/f/AI/OrcSDR-TEMP/orcmaps-quality-delivery/.local/global-assessment/global-z8-candidate.pmtiles .local/maps/labels-host/orcsdr_maps_tests /mnt/f/AI/OrcSDR-TEMP/orcmaps-quality-delivery/tests/fixtures/tiny-gzip.pmtiles /mnt/f/AI/OrcSDR-TEMP/orcmaps-quality-delivery/examples/m5stack-tab5/test-pack/springfield-97477.manifest.json .local/maps/fit-order-b59d478-20261020/orcmaps_world-before.bin
```

Before correction, the new inventory regression aborts at the assertion that an
installed pack remains eligible one camera level beyond its stored maximum.
Logs: `pack-overzoom-before-{build,test}-20261021.log`.

After correction: screen, pipeline, inventory and actual embedded-world checks
PASS. Inventory cases cover explicit preference, unavailable/out-of-coverage
preference, global/local source limits, minimum zoom, dateline and outside bounds.
Camera tests preserve projection at source z8 / camera z6, z8, z12 and z20,
including both sides of the dateline; invalid ranges/overflow are rejected.
Logs: `pack-overzoom-{build,test}-20261021.log`.

With the existing 543,380,195-byte global candidate, London, Tokyo and Nairobi
render at camera z9 and z12 using stored z8, with names, zero missing/failed
tiles and no bounded-stroke-capacity failures. Existing eight global reference
frames still pass. Logs: `pack-overzoom-real-{build,test}-20261021.log`.
These are bounded host reads of existing data, not a new download, full engine
suite or P4 timing measurement.

Native build/inspection, app installation and the actual SD global-pack device
test were open at this source checkpoint. Production map delivery, full setup
acceptance, LoRa interaction, FT8 runtime and the overall 33 ms gate remain open.

## Native and COM17 continuation — source `b326a123`

Executed from the OrcSDR worktree root:

```text
python .local/maps/build_fit_order_20261020.py build pack-overzoom-20261021
C:/Espressif/python_env/idf5.5_py3.14_env/Scripts/python.exe .local/maps/inspect_pack_source_20261021.py
python .local/maps/com17_label_bench.py logical-label-preflash-plan-20261021.json pack-source-preflash-20261021.log
python .local/maps/flash_pack_source_20261021.py
python .local/maps/com17_label_bench.py pack-source-device-plan-20261021.json pack-source-device-20261021.log
```

All return exit 0. Build and inspection logs:
`fit-order-native-pack-overzoom-20261021-20261020.log`,
`pack-source-native-inspection-20261021.log`.
App size 5,126,928 bytes; factory headroom 1,164,528 bytes.
App SHA-256: `ca8e2f97a4f2020860f580f4a4276a2b81b0b352f66006fd241a888b257d3842`.
ELF SHA-256: `18646a38e1ddc834ed8fca24518ab89e9f4ba08f9a3ba66da578a13ef003d57d`.
Inspection verifies existing symbol gates, unchanged 40 KiB DMA reserve,
checked PSRAM placement and absence of legacy-renderer symbols.

COM17 identity 30:ED:A0:E2:E6:95 is checked before every flash operation. Previous
app verification, fresh non-app backups, app-only write and preservation checks
PASS. Logs: `pack-source-{verify-previous,backup-*,verify-backups,app-write,preservation}-20261021.log`.
Artifacts/backups: `.local/maps/pack-source-b326a123-20261021/`; rollback is the
saved `1308e5bd` app. NVS, SD data, boot/table, world partition and C6 are preserved.
COM39 is untouched.

Native boot was captured with `idf.py -B build-native-hosted3 -p COM17 monitor
--no-reset`, followed by one explicit Ctrl-T/Ctrl-R reset. Invocation-directory log:
`apps/orcsdr-tab5/pack-source-boot-native-monitor-20261021.log`.
The matching ELF reaches SD/Wi-Fi/RTL ready. The serial device plan confirms
ADSB ownership, range changes 50→25→50, ready generation 3 and zero missing/failed
tiles. The new diagnostic reports camera z7 / source z4 / Firmware world overview.

This validates the installed embedded path and serial observability, **not** an
installed global z8 archive. That archive has only the host evidence above.
Bounded final snapshot: free PSRAM 20,694,612 bytes, largest DMA block 9,216 bytes,
main stack high-water mark 3,468 bytes. Driver is STREAMING; this does not close
the separately recorded decoder-queue-loss or sustained-radio acceptance gates.

The targeted UI source search no longer finds Lane County, Oregon, Springfield,
Portland, Eugene, Willamette or KEUG text. This excludes generated data, fixtures,
other applications and historical compatibility identifiers; it is not a whole
release-artifact audit. Existing user-installed regional packs are still valid.
The next device milestone is a verified compatible global/reference pack on SD,
followed by actual selection/overzoom under reception. Serial semantic-command
gaps are recorded in [the coverage ledger](2026-10-21-serial-control-coverage.md).
