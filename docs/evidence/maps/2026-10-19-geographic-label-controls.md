# Geographic label controls - source checkpoint, 2026-10-19

## Change

Picker, LoRa Map and ADS-B Map have visible Labels On/Off controls. Each stores
its own bool in dedicated NVS namespace `mapview`, using the existing NvsStore
commit/readback API. Failed saves leave the active choice unchanged and show an
error. Defaults preserve automatic labels. City names still begin at zoom 6.
FT8 has a separate preference identity, but its visible control/rendering remains
open with the migration from legacy world outlines; no FT8 completion is claimed.

The shared request/cache key includes label visibility. Cached previews cannot
cross a label-setting change. The worker disables both collection and text
rendering when labels are off, retaining geographic features, attribution and
caller-owned radio overlays. Legacy ORCMAP1 labels also honor the same setting;
its road/water/airport geometry and attribution remain. No pack format, receiver
lifecycle, location or radio-control behavior changed.

`RTL_MAP_STATUS` reports `ORC_MAP_LABELS picker=... lora=... adsb=... ft8=...`
without changing storage. This allows independent reboot checks through the
existing authenticated test session.

## Verification actually run

Invocation directory: F:/AI/OrcSDR-TEMP/maps-integration-completion.

- `cmake --build .local/maps/timing-host-20261019 --config Debug --target orcsdr_map_preferences_tests`: first failed because the new source/API was absent; then PASS.
- `cmake --build .local/maps/timing-host-20261019 --config Debug --target orcsdr_maps_tests orcsdr_map_preview_tests orcsdr_map_preferences_tests`: PASS.
- With `ORCSDR_LABEL_TEST_ARCHIVE=F:/AI/OrcSDR-TEMP/orcmaps-quality-delivery/.local/global-assessment/global-z8-candidate.pmtiles`, `ctest --test-dir .local/maps/timing-host-20261019 -C Debug -R '^map_(pipeline|preview|preferences)$' --output-on-failure`: **3/3 PASS**.
- `python .local/maps/check_map_transfer_syntax_20261019.py map_preferences.cpp map_service.cpp map_view.cpp map_screen.cpp lora_dashboard.cpp adsb_dashboard.cpp offline_map.cpp`: **7/7 P4 syntax PASS**.

Coverage: independent preference reload, defaults, failed open/write, invalid
consumer; actual picker touch/button and immutable camera; cached-preview label
mismatch rejection; real global archive label-off/on geometry and feature-count
preservation, zoom threshold and deterministic names. The recording font target
does not paint pixels, so equality proves unchanged basemap geometry, not an
on-device font comparison. Existing picker/setup/save checks also ran inside the
map-pipeline program. Its display/storage/navigation hooks are simulated.

The existing UI test needed current download-function stubs after the earlier
Get Maps change. It now also checks the explicit unconfigured fallback and
return to Installed. No production catalog, map upload or device change occurred
at this source checkpoint.

Logs in invocation directory: `map-label-preferences-{red,build,test}-20261019.log`,
`map-labels-host-{build,test,final-build,final-test}-20261019.log`, and
`map-labels-p4-syntax-20261019.log`.

## Remaining gates

Native link and COM17 control/persistence/overlay/radio checks are next. Physical
legacy-label and actual geographic-font acceptance remain open. FT8 basemap
migration/control, global quality, real HTTPS delivery and 33 ms interaction
remain open. COM39 is reserved for Dead Air and was not accessed.

## October 20 native and COM17 checkpoint

Firmware source `af877ed7`, app SHA-256
`a1f4ad51c0dfea54917b8edbc194a643b602eb856f470212187229054b33a738`.
The interrupted build had no live process; its existing log was preserved and
incremental work resumed with identical saved configuration/lock/C6 inputs.
Build and inspection PASS: 5,142,192-byte app, 1,149,264-byte partition headroom,
40 KiB DMA reserve, 1 KiB exception reserve, existing miniz/FT8 symbol gates,
PSRAM placements and unchanged component pin/lock. No production catalog input.

Commands from integration root:

- `python .local/maps/resume_map_labels_20261019.py`: PASS; appended to `map-labels-native-build-20261019.log`.
- `python .local/maps/inspect_map_labels_20261019.py`: PASS; `map-labels-native-inspection-20261020.log`.
- `python .local/maps/com17_label_bench.py map-labels-preflash-plan-20261020.json map-labels-preflash-20261020.log`: PASS, reception stopped before flash.
- `python .local/maps/flash_map_labels_20261020.py`: prior app/boot/table verification, fresh non-app backup/readback, app-only write and preservation PASS. Logs `map-labels-{verify-previous,backup-*,verify-backups,app-write,preservation}-20261020.log`.
- `python .local/maps/com17_label_bench.py map-labels-toggle-plan-20261020.json map-labels-toggle-20261020.log`: PASS; all three choices switched off independently, FT8 stayed on.
- `python .local/maps/com17_label_bench.py map-labels-persistence-restore-plan-20261020.json map-labels-persistence-restore-20261020.log`: PASS; off choices survived reboot, all three then restored independently to on.

COM17 identity `30:ED:A0:E2:E6:95`; prior firmware matched the saved download-flow
candidate. All non-app bytes verified unchanged after flash. Existing SD packs,
world partition, location and C6 were preserved. No COM39 access.
Native `idf.py -p COM17 monitor --no-reset` wrapper invoked from
`apps/orcsdr-tab5`, explicit Ctrl+T Ctrl+R reset and Ctrl+] exit, with logs saved
there as `map-labels-{boot,persistence}-native-monitor-20261020.log`.

Both boots reached SD/Wi-Fi/RTL ready. Wi-Fi first-attempt retries recovered;
the persistence boot also had a USB short-descriptor enumeration failure,
then the existing bounded dongle power-cycle recovery succeeded. This is a
recovered startup error, not a clean first-attempt USB claim. Historical saved
coredump was retained; no new panic observed in this bounded check.

At completion, Home graphics=1, original BROWSE 146.520 MHz restored, driver
STREAMING at 2,398,954 SPS with zero observed drops/overruns; C6/host 3.0.6 match.
Last health after visiting dashboards: free heap 4,235,520 B, internal free
15,287 B, largest internal/DMA block 9,216 B, PSRAM free 4,230,220 B/largest
2,359,296 B; main-stack low-water 1,996 B. These are workload-specific point
measurements, not a long soak or decoding-quality acceptance. Antenna suitability
was not established. Physical button/marker acceptance requested, still pending.

Retained-map follow-up from integration root:
`python .local/maps/com17_label_bench.py map-labels-retained-plan-20261020.json map-labels-retained-20261020.log`
PASS. Both map frames retained 2,835,456 B before/after stopped-radio SD read;
8,192 B matched SHA-256
`dc404a613fedaeb54034514bc6505f56b933caa5250299ba7d094377a51caa46`.
No new SD write. Radio resumed at BROWSE 146.520 MHz, 2,399,491 SPS, zero observed
drops/overruns, Home graphics=1, C6 3.0.6 match. After the LoRa/ADS-B/picker
workload, free PSRAM was 1,870,944 B/largest 917,504 B. This leaves limited
contiguous headroom for FT8 and requires its actual fallback/reclaim validation;
no general memory-health acceptance is claimed.
