# COM39 map candidate acceptance and enlarged selector

Date: 2026-10-17, America/Los_Angeles. Integration remains incomplete.

## Built/flashed source and preservation

Normal candidate `a32d2fd58c9d99737d74935e751a121b9400525a`:
5,021,216-byte app, SHA-256
`6d0f5cb42c249947857f8c4986be0f201bdf235906e9471677ac6ce3fc893fbe`.
Exact build command:
`& ./apps/orcsdr-tab5/tools/build-tab5-idf.ps1 -WorldMap F:/AI/OrcSDR-TEMP/maps-integration-completion/.local/maps/world/world-overview-z4.pmtiles`.
PASS: native linking, requested/locked/fetched OrcMaps agreement and bundled-miniz
symbol gate. Startup diagnostic OFF; 40,960-byte DMA reserve unchanged. Boot at
19:01 local reaches OrcSDR and reserves 40 KiB without the previous startup panic.

Approved target COM39, E8:F6:0A:E2:D6:03, Blog V3 and LoRa antenna. Fresh opaque
NVS/core/boot/table backups are retained under `.local/maps/com39-recovery/picker-20261017`.
Only boot/table/app were written. Flash hashes PASS; NVS, previous core and existing
world bytes verify unchanged before boot. No erase, format, SD pack replacement,
COM17 operation or C6 flash. Original full recovery and previous working candidate
remain local. Screenshots added to COM39 SD for the authorized UI tests are private.

## Actual device results

- PASS: full 900x330 picker instead of the erroneous 256x256 default. Camera pan
  and zoom produce new completed frames. Colors now match Settings charcoal,
  cyan controls and green primary actions.
- PASS: original configured coordinates retained after panning then Cancel,
  confirming the unchanged point, invalid manual edit, completing rerun/Quick Start,
  and an authenticated clean reset. Four pre-reset Settings captures are byte-for-byte
  identical. Post-reset Settings screenshot visually matches the private baseline.
  No private coordinates or SSID/IP are included here.
- PASS: rerun Welcome -> Network -> Location -> Offline Maps -> Complete;
  optional Network opens/closes back to setup; Back, Skip, Quick Start, Finish and
  Escape route. A '-' coordinate is rejected and Cancel retains the saved point.
  Finish saves completion; blank-device interrupted-setup recovery is still host-only.
- Inventory reports firmware world (zoom 0-4; partition capacity 1 MiB; runtime
  hash unchecked), legacy ORCMAP1 not installed, no arbitrary regional PMTiles.
  PREV/NEXT and rescan route. Existing map selection remains unchanged.
- OPEN: several captures lose headings, Cancel or button borders even though
  subsequent touches still route. An A/B on the same Complete screen, before and
  after releasing Map frame, shows the loss in the second capture. No screen
  navigation occurred between those two captures. This does not establish whether
  the physical panel is affected; the user could not check. Physical/display/capture
  acceptance remains open. Do not call this fixed by the color or camera changes.
- OPEN: SD_GET reports `esp_cache_msync invalid addr or null pointer`, SDMMC 0x102
  and read_failed while the map frame/cache is retained. One repeated bounded read
  fails; `RTL_MEM RECLAIM` releases 594,000 frame bytes plus associated pipeline
  allocations, then all seven screenshots download with hash verification, without
  reboot. Internal free rises 16,463 -> 27,827; largest block 10,240 -> 22,528.
  This is a repeatable workaround/evidence of map allocation pressure, not a root-cause
  fix. The map worker stack remains allocated. No SDK/SD/radio code was changed.
- Harness-only limit: the first post-reset SD_GET was correctly rejected radio_busy
  after boot auto-resumed reception. A bounded RTL_STOP, hash-checked fetch and
  LoRa resume completed it. Logs preserve the rejection. The intentional RTL_RESET
  uses OrcSDR's existing watchdog-backed clean-reset implementation, so reset_reason=7
  in that test is expected; it is not an unsolicited crash.

## ESP32-P4 timing while receiving

All following timings are the flashed 900x330 picker, not the new larger layout.
Four idle completed frame samples: 1,157,813-1,352,627 us total,
25,249-26,256 us display push; no missing or failed tiles.
Five distinct completed revisions sampled while receiving LoRa: total
2,115,508-2,321,570 us. Published map sprite pushes were 28,780/28,894 us.
Some completed revisions were superseded before pushing (push_us=0); busy responses
are nonblocking. Burst pan/zoom accepted newer requests; these observations do not
establish precise touch latency, p50/p95 map frame time or a target map FPS.

Representative loaded frame: tiles=4, features=632, reads=56, bytes=78036,
lookup_us=88, stream_us=661697, render_us=1645818, total_us=2321570, io_us=61414.
The 12-tile samples read 99,618 bytes. Source is the existing flash world partition,
not an SD PMTiles archive. `io_us/reads/bytes` therefore do not measure SD throughput.
`stream_us` includes decoding, internal reads and worker yields; it is not exclusive CPU.
Cached pushes reuse one completed sprite, not a tile-render cache.

Driver bytes 9,371,648 -> 183,402,496 during 19:11:01-19:12:31. Effective rate
at the last sample 959,840 sps; overruns=0, drops=0 at every status sample.
No USB error lines or unsolicited resets observed in this bounded capture. No CRC-valid
LoRa packets arrived, so actual decoding is not newly qualified. LoRa Map consumer
shows WAITING FOR VERIFIED POSITION because no selected node has a verified center;
its basemap/marker acceptance is still pending. Three RF Lab/Home/LoRa cycles route
successfully, but this is not physical visual-overlap acceptance or FM/audio testing.

Loaded memory at uptime 683,273 ms: internal_free=16,059, internal_largest=10,752;
DMA_free=1,863, DMA_largest=1,728; PSRAM_free=4,894,820,
PSRAM_largest=4,718,592. Main stack high-water mark=2,456 bytes.
UI_SELF_CHECK all six fields=1. Home: cadence_ms=33, 139 frames in 5,472 ms,
p50=38 ms, p95=44 ms (128-interval bounded window); about 25 updates/sec in this sample.
These are observed values, not a 30-FPS guarantee or an overnight soak.

## User-requested enlarged selector: prepared source only

Changes limited to `ui/map_screen.cpp` and `tests/maps/map_world_tests.cpp`:
1248x568 map at (16,72), compact coordinate fields in the header, zoom overlays
at the upper right, a centered crosshair and explicit confirmation in the footer.
Setup retains Back and a separate Skip. Overlay hit-testing precedes map panning,
and zoom overlays repaint after each asynchronous map push. The E7 editor, location
save logic, full DMA reserve and radio/USB/DSP code are unchanged.

Map area 708,864 pixels = 76.9% of the screen, 2.39 times the flashed layout.
RGB565 frame allocation rises 594,000 -> 1,417,728 bytes (+823,728 PSRAM bytes).
The existing allocation check/fallback remains. Larger frame times, DMA/SD effects,
physical visibility and touch behavior require the next approved candidate test.
This layout does not claim to fix the open capture or SD allocation issues.

Exact host command: `cmd /d /c .local\maps\run-pipeline.cmd`.
PASS: setup_flow, location_store, map_pipeline, world_geometry (ASAN); world fixtures
render deterministically at both 900x330 and 1248x568, with projection centered at
three test longitudes including the antimeridian. ESP32-P4 `-fsyntax-only` of
map_screen.cpp using its existing compile_commands entry: RESULT 0. No new link,
firmware build or flash was performed for this larger layout.

[Layout preview](2026-10-17-large-picker-preview.png) is a host-rendered world map
with illustrative controls, not a device screenshot. Synthetic fixture coordinates,
font metrics and square preview corners differ from the native display. It demonstrates
spacing only. Host render output and preview source remain local.

Next milestone: approve one native build/COM39 flash/test of the enlarged selector,
with preserved recovery/NVS/SD/C6 and COM17 untouched; then resolve capture/SD gates
before merge readiness. No main merge, push or release was performed.

## Saved logs (invocation directory)

All root logs under `F:/AI/OrcSDR-TEMP/maps-integration-completion/`:
`maps-picker-native-build-20261017.log`, `maps-picker-build-manifest-20261017.json`,
`maps-com39-picker-preflash-20261017.log`, `maps-com39-picker-backup-{nvs,core,boot,table}-20261017.log`,
`maps-com39-picker-flash-20261017.log`, `maps-com39-picker-preservation-20261017.log`,
`maps-com39-picker-boot-20261017.log`, `maps-com39-picker-initial-20261017.log`,
`maps-com39-picker-setup-20261017.log`, `maps-com39-picker-setup-fetch-20261017.log`,
`maps-com39-picker-reclaim-fetch-20261017.log`, `maps-com39-picker-loaded-20261017.log`,
`maps-com39-picker-capture-ab-20261017.log`, `maps-com39-picker-resume-20261017.log`,
`maps-com39-picker-persistence-boot-20261017.log`,
`maps-com39-picker-persistence-check-20261017.log`,
`maps-com39-picker-persistence-finish-20261017.log`, `maps-large-picker-host-20261017.log`,
`maps-large-picker-syntax-20261017.log`. Reviewed counter extract, manifest,
host/syntax logs and synthetic layout preview are beside this report.
Raw device logs, screenshots, opaque recovery images and key stay private/ignored.
