# COM17 catalog/time and Back-label candidate

At the user's request, updated COM17 while COM39 downloaded its map. No COM39
access occurred during this build/flash/check. Candidate source `90276ea5`
includes automatic catalog clock acquisition (`018b54ef`), the explicit FT8
unset-clock map test, and the downloader Back label (`90276ea5`).

Target: COM17, USB serial `30:ED:A0:E2:E6:95`, Blog V3c. App SHA-256
`ae5445f67442c912bcf07b72c428be3617788515c1cfd4ed33af75cf9919b45d`;
size 5,301,792 bytes. Existing C6 reports 3.0.6; no C6 update was issued.

## Actual checks

- Native build exit 0 and artifact inspection PASS. Existing 40 KiB internal
  reserve, PSRAM placement, production catalog/key, pinned engine and symbol
  gates retained.
- Verified previous app `6f625f66`, stopped reception, saved fresh non-app
  backups, flashed only the P4 app, then verified app plus NVS/PHY/coredump/world,
  bootloader and table preservation. SD was not erased or modified by test tools.
- Startup reached Ready with SD, Wi-Fi and receiver available. Wi-Fi first
  retried after disconnect reason 2, then connected; not reported as an
  uninterrupted first-attempt connection.
- Authenticated named `RTL_MAP OPEN DOWNLOADS` and `RTL_MAP DOWNLOAD CHECK`
  completed. `RTL_MAP DOWNLOAD LIST` reports phase ready, busy=0, ready=1,
  count=1: `global-regional`, 543,380,195 bytes. No map install was requested.
- Saved receiver location remained E7 `440628985,-1230160207`.
- A hash-verified PNG was inspected: BACK is present in the top-right navigation
  button, catalog row and Download/Resume controls visible and unclipped.
- Serial sessions closed; COM17 left on Get Maps. Physical user acceptance of
  the wording remains separate.

**Limit:** COM17 already had an established clock. This validates catalog access
on the combined candidate and its valid-clock path, not automatic SNTP acquisition
from an unset clock. The latter still has focused host coverage only; a controlled
unset-clock device check remains open. Do not claim that first-boot bug fully
hardware-validated from this successful catalog fetch.

## Exact commands and logs

Run directory `F:\AI\OrcSDR-TEMP\maps-integration-completion`; artifacts/logs
remain there outside Git.

- `python .local/maps/build_catalog_back_20261025.py build` — PASS;
  `catalog-back-native-build-20261025.log`.
- `python .local/maps/inspect_catalog_back_20261025.py` — PASS;
  `catalog-back-native-inspection-20261025.log`.
- `python .local/maps/flash_catalog_back_com17_20261025.py` — PASS;
  `catalog-back-{verify-previous,preservation}-20261025.log` and artifact
  `.local/maps/catalog-back-90276ea5-20261025/manifest.json`.
- Startup: `catalog-back-com17-boot-native-monitor-20261025.log`.
- `python .local/maps/com17_label_bench.py .local/maps/catalog-back-com17-check-plan-20261025.json catalog-back-com17-check-device-20261025.log` — completed.
- `python .local/maps/snapshot_catalog_back_com17_20261025.py` — PASS;
  `catalog-back-com17-snapshot-20261025.log`,
  `catalog-back-com17-screen-20261025.png`.

Next milestone: physically check the Back wording and automatic first-boot time
acquisition after scheduling around COM39's active download.
