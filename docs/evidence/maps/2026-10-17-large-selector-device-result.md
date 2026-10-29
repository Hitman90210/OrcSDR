# Enlarged selector: approved COM39 build/flash/test

Date: 2026-10-17, America/Los_Angeles. This completes the approved candidate test
cycle, not the complete maps integration or merge acceptance.

## Firmware provenance and preserved data

Source `6eb224662a6c6866268b7abda8c3fc3d7e3316ac`.
Exact native build command:
`& ./apps/orcsdr-tab5/tools/build-tab5-idf.ps1 -WorldMap F:/AI/OrcSDR-TEMP/maps-integration-completion/.local/maps/world/world-overview-z4.pmtiles`.
PASS: native build/link, factory size gate, dependency agreement, bundled-miniz
symbol verification (none of the symbols used by OrcMaps resolve to ROM).
Startup diagnostic OFF. Full 40,960-byte DMA reserve and RTL-SDR driver unchanged.
App size 5,023,568 bytes; factory headroom 1,267,888 bytes. SHA-256:
`bf32a59127bece56680da3a96bce7719fc7910662c2c4771bef61fdea8886d69`.
Boot ELF identity `5eb9c6011...` matches this build's reported boot.

The user restarted COM39 before any flash. Identity was rechecked, reception
started/stopped cleanly, and fresh backups were taken after that restart.
One harness assertion expected `state=idle`; firmware correctly reported `complete`
with `RTL_STOP_RESULT ESP_OK`. The expectation failure is retained in the log;
it is not a firmware fault or an attempt to flash while receiving.

COM39 E8:F6:0A:E2:D6:03 only. Fresh NVS/core/boot/table backups:
`.local/maps/com39-recovery/large-picker-20261017/`. Previous working a32d2fd5 app
and earlier full recovery remain private locally. Boot/table/app flash hashes PASS.
Unchanged NVS, prior core and world bytes verify before boot. No erase/format,
SD pack replacement, COM17 access or C6 flash. New screenshots were written only
as part of approved COM39 UI tests. Runtime journal/setup/location confirmation
writes occur normally after boot; NVS preservation is not a claim of postboot
byte-for-byte identity.

The world partition was intentionally provisioned earlier in this session and
verified again here, not written in this cycle. It is distinct from SD PMTiles
packs, the signed catalog and the legacy ORCMAP1 file. This test does not prove
that an installer omitting the world partition will supply an embedded world map.

Boot PASS at 20:11:31 local, with normal 40 KiB reservation. No unsolicited reset,
watchdog panic or USB error line observed in this bounded run. User's earlier
restart is distinguished from the monitored reset used to boot the new image.

## UI and persistence results

The 1248x568 map occupies 76.9% of the screen (2.39 times the original map area).
Its RGB565 sprite is 1,417,728 bytes. Coordinate fields and Cancel are above the
map, zoom overlays are inside its upper right, confirmation remains in the footer.
Screenshots visibly establish the larger map and Settings charcoal/cyan/green
palette. Zoom changes scale with the same displayed proposed E7 coordinates.
Pan changes the proposal. Escape/Cancel returns to Settings and preserves the
canonical receiver point. The original/cancel/after-confirm-and-invalid-edit Settings
screenshots are byte-for-byte identical. Private screenshots contain real location/
network details and are not committed or published.

Setup rerun: Welcome -> Network skip -> Location. The new footer SKIP reaches
Offline Maps. BACK returns to Location; USE LOCATION (unchanged original point)
reaches Offline Maps, then Next/Finish returns to Settings. The two inventory
captures match. Header latitude field opens the E7 editor; '-' input is rejected,
Cancel Field and Cancel retain the original location. This is not a blank-device
interrupted-setup/power-failure test; those remain host evidence only.

The user confirms all requested controls are visible on the physical COM39 screen:
both coordinate fields, Cancel, zoom +/-, and Use Location. This closes the requested
physical control-visibility check for the enlarged selector. Some saved captures
still omit coordinate fields or footer controls, including the setup location and
loaded-map captures. The capture discrepancy remains open; this confirmation does
not establish its root cause or qualify every screen state.

Three RF Lab/Home/LoRa navigation cycles complete with UI routing intact. Visual
absence of Home/RF Lab overlap is not newly physically accepted by this test.

## ESP32-P4 render and memory measurements

Archive is the existing Natural Earth overview in flash, zoom 0-4, engine
c3cc09de3882149e03c7c00d7897c6882b324c6b. No SD regional map archive was exercised.
All published frames sampled below report missing=0, failed=0.

| View / receiving state | Total render | Sprite push | Tiles / features |
|---|---:|---:|---:|
| Initial overview, reception stopped | 1,836,641 us | 60,767 us | 12 / 715 |
| Zoomed view, reception stopped | 3,440,274 us | 60,562 us | 24 / 1738 |
| Overview after zoom-out/pan, stopped | 1,908,793-1,935,395 us | 60,171-60,628 us | 12 / 715 |
| Sampled overview frames, active LoRa | 3,000,775-3,343,491 us | 68,504-69,224 us | 12 / 715 |
| Final stable zoomed view, active LoRa | 5,988,342 us | 69,327 us | 24 / 1738 |

The final zoom request was allowed to complete rather than superseded by a newer
pan/zoom. Its lookup_us=529, stream_us=2329138, render_us=3612408,
io_us=308874, reads=298, bytes=384594. Busy responses during worker rendering
are nonblocking. Burst requests supersede earlier renders; that is not a tile failure.
These are a handful of concrete view measurements, not map p50/p95/FPS or a proved
speed optimization. Different zoom/coverage makes old/new size comparisons unequal.
Cached pushes count one completed sprite reused, not a tile-render cache. Stream
time includes decoding/read work/yields, and read metrics are flash archive activity,
not SD bus throughput or exclusive CPU timing.

At final uptime 565,492 ms: internal_free=14,871, internal_largest=10,240;
DMA_free=1,327, DMA_largest=1,088; PSRAM_free=2,048,344,
PSRAM_largest=1,966,080; main stack high-water mark=2,456 bytes.
Historical minima during this run: internal_min=399, dma_min=219. These tight
margins are material limitations; passing a bounded test is not memory robustness
qualification. The larger frame uses 823,728 more PSRAM bytes than the 900x330 image.

## Radio and SD checks

COM39: Blog V3, LoRa antenna, 906.875 MHz, requested 960 ksps. During the primary
loaded capture, driver bytes grow 11,141,120 -> 173,899,776, ending at 959,838 sps;
all reported driver drops/overruns are zero. One live CRC-valid encrypted LoRa
message arrives during that bounded session: events=1, crc_ok=1, encrypted=1,
messages=1, failures=0, drops=0. It arrived after the pan/zoom portion, so this does
not claim a packet decoded concurrently with the heaviest zoom render. No test
transmitter was controlled. No plaintext/NODEINFO/position decoding is qualified
by an encrypted message; no verified-node map center was exercised.

After leaving the map, Home reports cadence_ms=33; 145 frames in 5,585 ms (about
26 updates/sec), p50=38 ms, p95=42 ms (128-interval window). UI_SELF_CHECK all six
fields=1. This is not a 30 FPS guarantee, FM/audio result, overnight soak or broad
full-suite regression qualification.

For the idle/setup screenshot downloads, Map frame was explicitly reclaimed before
SD_GET, using the earlier workaround. A separate post-RF SD_GET of the loaded-map
screenshot succeeded with the full map cache still allocated, with transfer hash
verification and no cache/SD error line. That one success does not close the prior
intermittent SD-read failure; no SDK/SD code was changed. Raw failing evidence from
the earlier a32d2fd5 cycle is preserved. No new SD pack was installed or modified.

Final state: enlarged selector open at the original stored point, zoomed once,
LoRa still receiving. Final driver sample: STREAMING, 959,720 sps, overruns=0,
drops=0; LoRa crc_ok=1. Unconfirmed proposals were not saved during the loaded pan
run. No monitor/transfer is left owning COM39.

## Commands, saved logs and next milestone

Existing authenticated `.local/maps/com39_bench.py`, no PAIR/key replacement:
`python .local/maps/com39_bench.py PLAN.json LOG.log`. Plans:
`large-preflash-plan.json`, `large-after-user-restart-plan.json`, `large-idle-plan.json`,
`large-setup-plan.json`, `large-loaded-plan.json`, `large-held-read-plan.json`,
`large-final-plan.json`. Idle/setup/loaded/held-read/final plans all complete with
expected responses. Raw NVS, keys, packet identities and coordinates remain private.

All logs were saved in invocation root `F:/AI/OrcSDR-TEMP/maps-integration-completion/`:
`maps-large-picker-native-build-20261017.log`, `maps-large-picker-build-manifest-20261017.json`,
`maps-com39-large-preflash-20261017.log`, `maps-com39-large-user-restart-20261017.log`,
`maps-com39-large-backup-{nvs,core,boot,table}-20261017.log`,
`maps-com39-large-flash-20261017.log`, `maps-com39-large-preservation-20261017.log`,
`maps-com39-large-boot-20261017.log`, `maps-com39-large-idle-20261017.log`,
`maps-com39-large-setup-20261017.log`, `maps-com39-large-loaded-20261017.log`,
`maps-com39-large-held-read-20261017.log`, `maps-com39-large-final-20261017.log`.
Reviewed native-build log, counter-only extract and manifest are beside this report.
No source changes or additional firmware builds followed this tested commit.

Next integration milestone: resolve the saved-capture discrepancy and the
retained-map/SD allocation boundary before merge readiness.
Regional SD PMTiles, compatible-dashboard verified-position overlays, physical
no-SD/blank-device recovery and release installation remain distinct acceptance gates.
Main was not merged; no push, release or PR merge was performed in this cycle.
