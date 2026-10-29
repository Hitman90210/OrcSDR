# COM39 placement candidate and picker follow-up — 2026-10-17

## Candidate actually built and flashed

Source 7c878d4c922c438272d3ad5e936b8580a14f5917. Native command:
`& ./apps/orcsdr-tab5/tools/build-tab5-idf.ps1 -MapStartupDiag -WorldMap F:/AI/OrcSDR-TEMP/maps-integration-completion/.local/maps/world/world-overview-z4.pmtiles`.
PASS: 5,020,800-byte app, SHA-256
`04733a8c2475e43ecf3596c107543ddb1343c423d05cfb6f685e6d9ed8f301a0`.
The linked placement recovers 1,464 internal data/BSS bytes. Dependency pin and miniz
symbol gates pass. This is a diagnostic bench image, not a release qualification.

Approved target: COM39, E8:F6:0A:E2:D6:03. Fresh opaque NVS/core backups saved
locally. App, bootloader and table writes verified; unchanged NVS, previous core
and existing world partition verified before reboot. No SD packs overwritten,
NVS erased or C6 flashed. COM17 was not touched. Screenshots were added on COM39's
SD only for the approved UI tests; downloaded copies remain private/ignored.

Boot PASS at 18:43:43 local: full 40,960-byte DMA reservation succeeds, largest DMA
block before reserve 43,008 bytes. Original and candidate Settings screenshots
show the same stored receiver coordinates, label, map choice and range. Private
screenshots and NVS backups are not published. Legacy configured boot reaches Home.

## Bounded hardware checks and limits

Commands used via the authenticated local `com39_bench.py` harness:
`RTL_UI OPEN/TAP/STATUS`, `RTL_KEYBOARD KEY ESC`, `FT8 SHOT`, hash-checked `SD_GET_*`,
`RTL_HEALTH`, `RTL_LISTEN LORA`, `RTL_DRIVER STATUS`, `RTL_CAPTURE_STATUS`,
`RTL_LORA_STATS`, `RTL_LORA_NATIVE_STATUS`, `RTL_MAP_STATUS`, `UI_SELF_CHECK`.
Plans are ignored local JSON files; all raw logs were saved in the invocation root.
Authentication reused the existing key, never PAIR. Initial harness attempts expired
around screenshots; logs preserve those failures. Re-authentication before each step
completed the continuation plans. These failures are not firmware resets.

- Setup rerun opens Welcome. Network Settings opens and closes back to maps.
  Next/Skip reaches Location, Offline Maps, Complete; Back and Escape return.
  No new location, map choice or completion record was saved in these checks.
- Inventory visibly reports two entries: usable firmware world (zoom 0-4, 1 MiB
  partition capacity, hash unchecked at runtime) and legacy Lane County NOT INSTALLED.
  No arbitrary SD PMTiles archives are shown. PREV/NEXT and rescan route successfully.
  Catalog offers remain separate from installed data.
- Acceptance FAIL: picker shows a 256x256 map in a 900x330 region. The user's photo
  confirms it. Reproduction with the exact calling bounds prints
  `begin=0 width=256 height=256 zoom=0`: GeoBounds is longitude-first, but the caller
  supplied latitude first and ignored Begin failure. Pan/zoom/location confirmation
  are not accepted until corrected firmware is tested.
- User rejects the saturated blue setup background. Follow-up uses the existing
  Settings charcoal 0x0841/panel 0x1082/cyan 0x04ff/green 0x6fe8 palette.
- Some setup screenshots omit the title, Cancel or bottom navigation controls.
  Input routing still worked, but this is not visual acceptance. Recheck redraws
  and optional Network return on the next candidate before calling setup complete.
- `RTL_MAP_STATUS` returns incomplete/missing lines in several captures, while
  ordinary console status commands respond. Its stdio path differs from OrcSDR's
  installed USB console writer. Follow-up sends map diagnostics through that
  established writer, with no global console/driver change. Timings remain unverified.
- LoRa antenna and RTL-SDR Blog V3 on COM39: active 906.875 MHz, 960 ksps. Three
  RF Lab -> Home -> LoRa cycles completed. Driver bytes grew 7,208,960 -> 66,191,360;
  effective_sps at the second snapshot 959,768, overruns=0, drops=0. No CRC-valid
  packets arrived in this short window, so actual decoding is not requalified.
- At uptime 663,313 ms: internal_free=16,095, internal_largest=10,752,
  DMA_free=1,543, DMA_largest=1,152, PSRAM_free=4,911,544,
  PSRAM_largest=4,587,520. Main stack high-water mark=1,628 bytes. Tight DMA margin
  remains a measured limitation. UI_SELF_CHECK all six fields = 1; no reset observed.
  Device left receiving LoRa on Home. This is a short continuity check, not a soak,
  map frame-rate measurement, FM/audio result or visual overlap acceptance.

## Prepared follow-up, not yet built/flashed

One shared tested picker initializer uses named bounds, checks Begin, and preserves
camera dimensions/selection. Map UI rejects movement on a failed initializer.
Regression checks cover actual full-size world rendering, zoom preserving selection,
recenter, invalid dimensions/coordinates and the existing allocation-recovery cases.
The map screen uses Settings colors, filled charcoal buttons and green primary actions.
The only main.cpp change is the map report callback; its pre-existing NUL byte count
is unchanged. Radio/DSP/USB/Wi-Fi algorithms and full DMA reserve remain unchanged.

Exact targeted command: `cmd /d /c .local\maps\run-pipeline.cmd`.
PASS: setup_flow, location_store, map_pipeline, world_geometry, all four with ASAN.
The initial new target failed to link before its helper was implemented; the separate
old-caller reproduction above is the behavioral failure evidence. Final log:
`maps-picker-regression-final-20261017.log`. No new native image or flash was run
for this follow-up. Native linking, full-size P4 render, colors, screen completeness,
map timing output and loaded-RF profiling remain gates.

Next milestone: one approved normal native build (startup diagnostic OFF) and
COM39 flash/retest of this focused follow-up, preserving recovery/NVS/SD/C6 and leaving COM17 untouched. Do not merge
main or declare integration complete on these results.

## Invocation-root logs

`maps-placement-fix-build-20261017.log`, `maps-com39-placement-preflash-20261017.log`,
`maps-com39-placement-backup-nvs-20261017.log`, `maps-com39-placement-backup-core-20261017.log`,
`maps-com39-placement-flash-20261017.log`, `maps-com39-placement-preservation-20261017.log`,
`maps-com39-placement-boot-20261017.log`, `maps-com39-placement-initial-20261017.log`,
`maps-com39-placement-picker-20261017.log`, `maps-com39-placement-flows-20261017.log`,
`maps-com39-placement-flows2-20261017.log`, `maps-com39-placement-radio-20261017.log`,
`maps-picker-red-20261017.log`, `maps-picker-regression-red-20261017.log`,
`maps-picker-regression-green-20261017.log`, `maps-picker-regression-final-20261017.log`.

Reviewed counter-only extract and JSON are beside this note. Raw device logs,
screenshots, recovery images, keys and location baseline remain local.
