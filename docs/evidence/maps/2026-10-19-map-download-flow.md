# Map catalog/download UI and worker connection — 2026-10-19

## Scope and behavior

The existing map worker now handles bounded HTTPS catalog fetch, signature/trust
acceptance, archive transfer, verification, activation and recovery. No new task
or dashboard-owned renderer was added. Catalog entries include sizes, region,
zoom, recommendation and installed availability. Setup and Local Maps share the
Get Maps list with progress, Stop Transfer and explicit Retry. Returning to
Installed preserves the existing explicit Use This Map action. Location and radio
settings are not changed by an installation.

Rescan retries only durable pending activation; it never repeats an HTTP catalog
or archive operation. Failed/cancelled pending installs remain retryable offline.
The shared UI/worker eligibility functions prevent a pending intent from being
mistaken for active work, which previously hid Retry.

The catalog base HTTPS directory and public-only P-256 PEM are explicit CMake
inputs, ORCSDR_MAP_CATALOG_BASE_URL and ORCSDR_MAP_CATALOG_PUBLIC_KEY_FILE.
Both default empty. No production endpoint/key is selected; the screen states
that downloads are unconfigured and installed/embedded maps remain available.
The expected assets are maps-catalog-v1.json and maps-catalog-v1.sig. HTTPS,
trusted UTC, signature validation and persisted revision/hash are required
before exposing remote entries. Limits are 128 KiB JSON and 128 bytes signature,
including chunked replies. Config shape tests do not prove cryptographic validity.

Already-owned complete archives are fully reverified without redownloading.
A missing owned archive no longer appears installed; matching owned metadata
allows verified staged data to be renamed only into the missing archive path.
Existing files are never overwritten on this repair path. The durable install
intent makes a cut immediately after archive publication recoverable offline.

## Tests actually run

Invocation directory: F:/AI/OrcSDR-TEMP/maps-integration-completion.

- `cmake --build .local/maps/catalog-security-20261019 --config Debug --target map_transfer_test map_catalog_fetch_test`
- `cmake --build .local/maps/catalog-security-20261019 --config Debug --target map_download_actions_test`
- `ctest --test-dir .local/maps/catalog-security-20261019 -C Debug -R '^map_(transfer|download_actions|catalog_fetch|delivery_state|delivery_nvs)$' --output-on-failure`: **5/5 PASS**.
- `python apps/orcsdr-tab5/tests/maps/catalog_security/catalog_config_test.py`: **6/6 PASS**, default offline, configured public source, paired inputs, unsafe URL and private/oversized key rejection.
- `python .local/maps/check_map_transfer_syntax_20261019.py map_catalog_fetch.cpp map_downloads.cpp map_install.cpp map_activation.cpp map_service.cpp map_screen.cpp`: **6/6 PASS**, actual P4 SDK flags/headers.
- `ctest --test-dir .local/maps/catalog-security-20261019 -C Debug -R '^map_transfer$' --output-on-failure`: **1/1 PASS** after adding the repair-publication interruption scenario.

The missing-archive repair test failed before its correction. The already-owned
no-network test also failed before its fast path was implemented. New fetch and
eligibility tests initially failed to build because the implementations/APIs were
absent. Tests use simulated network/filesystem/NVS boundaries, not physical SD
power loss. Scoped read-only review identified the pending-Retry and missing
archive blockers, then confirmed their corrections with no further verified
blocker in that scope.

Logs are in the invocation directory:
`map-download-flow-{host-build,host-test,regressions}-20261019.log`,
`map-download-actions-{red,build,test}-20261019.log`,
`map-catalog-config-test-20261019.log`, `map-transfer-p4-syntax-20261019.log`,
`map-install-idempotent-{build-red,red}-20261019.log`,
`map-install-repair-{build-red,red,build}-20261019.log`, and
`map-install-repair-interruption-{build,test}-20261019.log`.

## Open gates

Native linking, PSRAM/stack/memory inspection and COM17 HTTPS/SD/NVS/UI/radio
validation remain unproven at this source checkpoint. COM17 remains on its last
accepted picker app; COM39 was not accessed. No global pack installation,
publication, production signing, source push or main merge occurred.

The wider goal still requires dashboard migration/label controls, global visual
acceptance, hosting approval and hardware delivery/fault testing. The 33 ms
interaction target remains unmet.

## Subsequent native and COM17 checkpoint

Source/build HEAD `5052153e` (firmware implementation `21c5c5a2`), clean tracked
source. Native build and inspection PASS:

- `python .local/maps/build_download_flow_20261019.py`
- `python .local/maps/inspect_download_flow_20261019.py`
- App 5,140,736 bytes; factory headroom 1,150,720 bytes (18%).
- App SHA-256 `136a710fa8655725675c66769ffa5f765143e80d8f55b0087514d0ffac142052`.
- Download entries/catalog/snapshot: 7,168/64/128 bytes in `.ext_ram.bss`.
- 40 KiB DMA reserve, 1 KiB emergency exception pool, saved SDKCONFIG, diagnostics
  OFF, unchanged engine pin/lock and C6 3.0.6 image input verified. Existing miniz
  and FT8/FT4/JS8 PSRAM symbol gates PASS. Catalog source remains empty.
- Build/inspection logs: `map-download-flow-native-{build,inspection}-20261019.log`.
- Sealed local artifact/manifest: `.local/maps/download-flow-21c5c5a2-20261019/`.

COM17 USB serial `30:ED:A0:E2:E6:95` verified. The running prior app bytes matched
accepted `aca08b75`; bootloader and partition table matched saved recovery. Fresh
non-app backups were read and verified. P4 app-only write/readback PASS; all
non-app bytes still matched immediately afterward. C6 was not flashed and SD
packs were not written. The first flash script stopped before any write because
esptool 5.3.1 changed MAC-line spacing; its exact logged MAC was verified and a
whitespace-tolerant anchored guard was used for the successful attempt.

Commands/logs, integration-root invocation:

- `python .local/maps/com17_label_bench.py map-download-flow-preflash-plan-20261019.json map-download-flow-preflash-20261019.log`: PASS; reception stopped before flash.
- `python .local/maps/flash_download_flow_20261019.py`: app-only write and preservation PASS; logs `map-download-flow-v2-*-20261019.log`. Original pre-write guard log retained as `map-download-flow-verify-previous-20261019.log`.
- Native monitor launched from `apps/orcsdr-tab5` using `python F:/AI/OrcSDR-TEMP/maps-integration-completion/.local/maps/monitor_preview_strip_20261019.py`, `idf.py -p COM17 monitor --no-reset`; one explicit Ctrl+T Ctrl+R boot after flash, Ctrl+] exit. Log in that invocation directory: `map-download-flow-boot-native-monitor-20261019.log`.
- `python .local/maps/com17_label_bench.py map-download-flow-postflash-plan-20261019.json map-download-flow-postflash-20261019.log`: PASS.
- `python .local/maps/com17_label_bench.py map-download-flow-retained-ui-plan-20261019.json map-download-flow-retained-ui-20261019.log`: retained-map SD read and Home return passed; later navigation assertion incorrectly expected `route=maps`. Actual screen was correctly `screen=maps, route=home`; the route describes the radio UI.
- `python .local/maps/com17_label_bench.py map-download-flow-ui-followup-plan-20261019.json map-download-flow-ui-followup-20261019.log`: corrected remaining checks PASS.

Boot reached ready with SD, Wi-Fi and RTL connected. C6/host 3.0.6 match. One
transient C6 SDIO initialization retry and one Wi-Fi authentication retry
recovered; no new panic/reset observed in this bounded session. Existing saved
coredump remained present and is historical, not evidence of a new crash.

Existing picker frame: 9 tiles, zero missing/failed, 413 features; both map frames
retain 2,835,456 bytes PSRAM. A stopped-radio 8,192-byte SD readback matched
`dc404a613fedaeb54034514bc6505f56b933caa5250299ba7d094377a51caa46`, with the same
map allocation before/after. Reception resumed at the original BROWSE 26.620 MHz
setting. Active driver near 2.4 MSPS, zero observed drops/overruns in bounded
checks. Antenna suitability is unverified; no RF decoding quality claim.

Last health sample: uptime 148,838 ms, free heap 19,062,508 bytes, internal free
15,575 bytes, largest internal/DMA block 9,728 bytes, PSRAM free 19,057,072 bytes,
main stack low-water 3,692 bytes. These are point-in-time measurements, not an
HTTP/TLS memory acceptance result. Physical Get Maps readability was requested
and is pending. No actual catalog HTTP request, global-pack installation,
physical SD power-loss recovery, main merge or publication is claimed.
