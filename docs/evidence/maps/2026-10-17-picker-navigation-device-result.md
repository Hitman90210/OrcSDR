# Picker navigation and changed-location save: COM39 result

Date: 2026-10-17, America/Los_Angeles.
Tested firmware source: `048988919cbd7ad70d21ad466a5c0bcfe30650a6`;
implementation commit `ef75604818c4a710cd0f46a04c4207f0caa18e73`.
This closes the approved native build/COM39 cycle, not the complete maps integration.

## Build, target and preservation

Exact native command, run from `F:/AI/OrcSDR-TEMP/maps-integration-completion/`:

```powershell
& ./apps/orcsdr-tab5/tools/build-tab5-idf.ps1 -WorldMap F:/AI/OrcSDR-TEMP/maps-integration-completion/.local/maps/world/world-overview-z4.pmtiles
```

PASS: native build/link, partition size check and bundled-miniz symbol gate.
App 5,025,248 bytes; factory headroom 1,266,208 bytes. App SHA-256:
`fd2693d0cce513cacc2925213c6d142132df9707574852ffbddad74e17e9bc01`.
ELF SHA-256 `59a272693b5f70a57b1814c55a4bf956c14bb86206461c3cd3d5006053341300`
matches both boot captures' `59a272693...` identity. Startup diagnostics OFF;
full 40,960-byte internal DMA reserve unchanged. Existing compiler warnings were
retained; this is not a warning-free build claim.

COM39 USB serial E8:F6:0A:E2:D6:03 verified before device operations. Fresh opaque
NVS/core/boot/table backups and hash manifest are private under
`.local/maps/com39-recovery/navigation-picker-20261017/`. Prior working
`6eb22466` app/ELF and earlier full recovery retained. Existing partition table
matched the candidate. Only boot/table/app regions were flashed; all written
hashes verified. NVS, previous core dump and existing world partition verified
unchanged before boot. No SD pack replacement, erase/format, C6 flash or COM17
access. Normal runtime setup/journal/location writes are distinct from flash
preservation; NVS is not claimed byte-identical after the save tests.

World partition was provisioned earlier, not installed in this cycle. Firmware
world, SD regional PMTiles, signed catalog and ORCMAP1 remain distinct.

## Changed files and behavior

Implementation files are `apps/orcsdr-tab5/ui/map_screen.cpp`,
`map_pipeline.cpp/.hpp`, `map_service.cpp`, `settings_app.cpp`,
`location_store.hpp` and six changed lines in `main.cpp`; map test CMake,
pipeline/world/storage tests and new screen-test/device stubs are in
`apps/orcsdr-tab5/tests/maps/`. No radio/USB/Wi-Fi/DSP restructuring.

- 1248x568 picker starts at zoom 3, one level closer, retains its zoom-out floor.
- Four visible pan arrows, plus existing tap/drag, move across the world.
- World-only fallback uses one coarser source level, enlarged in vector space
  at full screen resolution. Regional source policy is unchanged. This reduces
  some geographic detail; it is not a rendered-tile or adjacent-page cache.
- Receiver Location has a prominent 888x68 green map button. The ADS-B range row
  is removed there; the actual radar control and stored range are retained.
- Coordinate-only Settings updates repaint; saves read the record back before
  publishing success and emit a coordinate-free diagnostic marker.

Earlier host/syntax result: [four targeted tests and P4 syntax](2026-10-17-picker-navigation-save-host.md).
No further source edits or firmware builds followed that approved candidate.

## Actual device checks

Normal boot and one deliberate save-persistence reboot PASS, both with normal
40 KiB reserve. UI_SELF_CHECK reports am/fm/home/shortwave/keypad/editor all 1.

Preflash Receiver Location was captured live. It differed from an older session
note, so the live preflash point, not the older note, was used for restoration.
Private screenshots and original-point JSON contain location/network details;
they are not committed or published.

New Settings screenshot shows the large button and no ADS-B range row. Picker
screenshots show the closer world view, coordinate fields, Cancel, +/- and arrows.
User confirmation during reception: **"Controls and map detail look good"**.

Idle checks exercised both zoom directions and all four pan arrows. Cancel
retained the initial point (coordinate-field pixel crops match the preflash
screen exactly). Then a small map tap proposed a different longitude:

1. Use Location emitted `ORC_MAP_LOCATION_SAVE OK verified`.
2. Receiver Location displayed the changed longitude; saved coordinate-field
   crops differ from the original, and the screen text matches the proposal.
3. A deliberate reboot retained those same coordinate-field pixels exactly.
4. The original live point was restored using the existing location command.
   Restored coordinate-field crops match the initial screen exactly.

Changed-point save/display/reboot persistence therefore pass on this device.
The cause of the user's earlier failure is not proven; this does not claim every
touch sequence, interrupted write or power-loss case has been qualified.

One screenshot acknowledgement took longer than the plan's four-second wait,
so that plan stopped with a missing-ack assertion after the successful save.
A follow-up retrieved both files with verified transfer hashes; the saved
Settings screenshot existed and showed the changed point. Later screenshot
waits were eight seconds. The failure is retained, not counted as an initial
plan pass or a location-save failure.

## Map performance while receiving

COM39: Blog V3, previously connected LoRa antenna, 906.875 MHz, requested
960 ksps. No antenna swap or transmitter control in this cycle.

| Camera/view | Reception stopped | Active LoRa |
|---|---:|---:|
| Starting view (zoom 3), original point | 859,235 us | 1,510,223-1,773,451 us |
| One level closer (zoom 4) | 1,330,400 us | 2,474,071 us |
| Half-screen pan (zoom 3) | 1,533,454 us | 2,677,362 us |

Starting view uses 9 tiles / 413 features; zoomed uses 6 / 690; panned uses
12 / 715. Sampled frames have missing=0, failed=0. Sprite pushes take about
60-61 ms idle and 68-69 ms receiving. Frame remains 1,417,728 PSRAM bytes;
no second framebuffer or new tile cache added. These are individual render
samples, not p50/p95/FPS or a general latency guarantee.

Prior `6eb22466` loaded renders were approximately 3.0-3.4 seconds overview and
6.0 seconds zoomed. The new policy is faster in these samples, but initial zoom,
stored latitude and source detail differ. Do not publish an exact controlled
speedup percentage. Physical appearance of the coarser world was accepted by
the user. Regional SD performance was not exercised.

Primary loaded capture: driver bytes 11,141,120 -> 166,166,528, ending effective
959,978 sps; every sampled driver overrun/drop counter is zero. One CRC-valid
encrypted packet decoded early in this session, before map rendering began;
it does not prove decoding during the heaviest redraw. No plaintext/NODEINFO/
verified-position overlay acceptance follows from the encrypted packet.

RF Lab -> Home navigation completed. Home measured 174 frames / 7018 ms,
cadence=33 ms, p50=40 ms, p95=45 ms, window=128. This is about 24.8 updates/sec,
not a 30 FPS guarantee, FM/audio soak or broad regression qualification.

## SD, final observation and open limits

Initial preflash screenshot download was correctly rejected as `radio_busy`;
the download succeeded after stopping reception. Idle/save downloads explicitly
reclaimed the map sprite using the previously documented workaround.

Dedicated loaded screenshot read: reception stopped, **full map cache retained**,
SD_GET succeeded with its SHA-256 verified and no cache/SD error line. This is
another passing sample, not closure of the prior intermittent SD-read failure.
No SDK/SD code changed. New viewed captures include the controls, but an older
intermittent capture discrepancy has not been root-caused by this patch.

At the final read-only observation (21:27:46 local), uptime=2,432,380 ms; map still
open at the restored original point and LoRa streaming. The period after the
bounded tests was passive operation, not an intentionally instrumented soak.
Final driver: 4,114,350,080 bytes, 959,762 sps, overruns=0, drops=0.
LoRa: events=27, crc_ok=18, encrypted=17, messages=17, failures=9, drops=0.
Those nine reported decoder failures remain recorded; their RF/protocol causes
were not investigated and are not classified as firmware regressions or successes.
Manual gain was observed at the last read; the final check did not change gain.

Final sampled memory: internal_free=14,943, internal_largest=8,704;
DMA_free=1,595, DMA_largest=1,280; PSRAM_free=2,048,224,
PSRAM_largest=1,966,080; main stack high-water mark=3,544 bytes.
Historical minima internal=691, DMA=179 bytes. These tight margins remain open;
bounded boot/UI/stream passes do not qualify memory robustness. Uptime and reset
reason are consistent with the intentional persistence reboot; no new reset is
indicated by the final observation. The intervening period was not continuously
captured, so absence of every possible transient error is not established.

COM39 left showing the enlarged map at the original point with LoRa receiving.
No monitor or transfer holds the serial port. COM17/C6 untouched. No merge/push.

Next integration milestone: resolve map/SD allocation and memory boundaries, then
regional PMTiles and compatible-dashboard verified-position tests. Blank-device,
interrupted setup, no-SD physical recovery and release installation remain
separate acceptance gates. No additional native build is authorized by this report.

## Commands and saved logs

Device plans used the existing authenticated harness, no PAIR/key replacement:

```text
python .local/maps/com39_bench.py PLAN.json LOG.log
python .local/maps/flash-navigation.py
python .local/maps/capture_nav_boot.py BOOT_LOG.log
python .local/maps/check_nav_location.py BEFORE.bmp AFTER.bmp same|different
```

Native Python was `C:/Espressif/python_env/idf5.5_py3.14_env/Scripts/python.exe`.
Plans under `.local/maps/`: nav-preflight-plan, nav-preflight-stop-plan,
nav-idle-plan, nav-save-plan, nav-save-fetch-plan, nav-after-reboot-plan,
nav-restore-plan, nav-loaded-plan, nav-read-finish-plan, nav-final-observe-plan
(all `.json`). All completed except the two explicitly recorded preflight
radio-busy and save screenshot-wait failures. Private tooling and raw logs remain
local; NVS/secrets/coordinates/packet identities were not published.

Raw logs are saved in the invocation directory
`F:/AI/OrcSDR-TEMP/maps-integration-completion/`:

- `maps-picker-navigation-native-build-20261017.log` and preflash build manifest.
- `maps-com39-nav-{preflight,preflight-stop,backup-nvs,backup-core,backup-boot,backup-table,flash,preservation,boot,idle,save,save-fetch,persistence-boot,after-reboot,restore,loaded,read-finish,final-observe}-20261017.log`.
- `maps-com39-nav-{location-preservation,changed-point,reboot-location-proof,restored-location-proof}-20261017.log`.

Reviewed counter-only extracts and device manifest are beside this report. Full
raw captures contain private network and receiver information and stay uncommitted.
