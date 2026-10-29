# Map picker navigation and save follow-up

Date: 2026-10-17, America/Los_Angeles.
Firmware source changes: `ef75604818c4a710cd0f46a04c4207f0caa18e73`.
Baseline: `f7a93879`, following the tested COM39 image `6eb22466`.
This report is host/P4 syntax evidence, not a new device acceptance claim.

## Changes

- `ui/map_screen.cpp`: four on-map pan buttons move half a viewport at a time;
  drag/tap and keyboard controls remain. Overlay presses cannot start a drag.
  A missing proposal now produces guidance instead of silently ignoring Use Location.
- `ui/map_pipeline.cpp/.hpp`: picker starts one zoom level closer while retaining
  its old zoom-out floor. World fallback uses one coarser archive level, with tile
  size doubled to preserve geographic coverage, selected point and full pixel
  resolution. At high zoom the existing world archive maximum still applies.
  This trades some coastline/detail precision for less work. Regional PMTiles
  source selection is unchanged. This is not a neighboring-page or tile cache.
- `ui/map_service.cpp`: shared adapter applies that world-only source-level policy;
  no dashboard implements its own map renderer. Existing worker yields and radio
  ownership are unchanged; no second framebuffer or new cache was allocated.
- `ui/settings_app.cpp`: prominent 888x68 green Choose Location on Map button
  below the coordinate fields. ADS-B range removed from this general location page;
  ADS-B radar controls and stored range remain. Coordinate/configuration/label/map
  changes now trigger a Settings repaint, not only lookup-status changes.
- `ui/location_store.hpp` and six changed lines in `ui/main.cpp`: read back the
  coordinate record before publishing success. Existing atomic blob, migration and
  rollback behavior remain. Console reports `ORC_MAP_LOCATION_SAVE OK verified`
  or `ERROR storage_verification` without printing private coordinates.
- `tests/maps`: storage read-back fault check, world projection/coverage checks,
  and actual picker/Settings UI code exercised with fake device interfaces.
  Test support headers are confined to the host target; native firmware does not
  include them. CMake, pipeline/world/storage tests and the new screen test changed.

## Actual verification

Targeted host build (Release, existing MSVC AddressSanitizer configuration):

```powershell
cmake --build .local/maps/pipeline-host --config Release --target orcsdr_maps_tests orcsdr_location_store_tests
cmake --build .local/maps/pipeline-host --config Release --target orcsdr_setup_tests
$env:PATH='C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC/14.50.35717/bin/Hostx64/x64;'+$env:PATH
ctest --test-dir .local/maps/pipeline-host -C Release -R '^(location_store|setup_flow|map_pipeline|world_geometry)$' --output-on-failure
```

Final result: **4/4 PASS**, 1.24 seconds. No sanitizer finding in this bounded run.
The screen test runs within map_pipeline/world_geometry, not as an additional
fifth CTest test. It verifies a changed-point save, returning to Settings with that
longitude displayed, rejected storage writes, Cancel preserving the saved point,
arrows, zoom preserving selection, coordinate-only Settings repaint and explicit
guidance when no point was chosen. Storage/navigation/display are faked; the real
main.cpp callback, physical NVS and panel are not executed by this test.

World tests cover 900x330 and 1248x568 at three longitudes, including the date line.
Requested and coarser-source views project sample coordinates to the same pixels.
All sampled coarser renders report missing=0 and failed=0. At 1248x568 the samples
decode 9-12 tiles and 413-715 features. These counts are not ESP32 timing results.

P4 syntax-only command:

```powershell
& C:/Espressif/python_env/idf5.5_py3.14_env/Scripts/python.exe .local/maps/check_picker_navigation_syntax.py
```

PASS for map_screen.cpp, map_service.cpp, map_pipeline.cpp, settings_app.cpp and
main.cpp using the existing native compile database and real device headers.
This does not link firmware or measure its size. Existing upstream literal-suffix
and main.cpp NUL-literal warnings remain outside this patch.

Initial host attempts caught a test-double namespace collision and missing stub;
these were corrected. The first test launch did not execute assertions because
the sanitizer DLL was outside PATH and the setup Release executable was absent.
The final build/runtime above resolves those harness failures; their logs remain.

Logs saved in invocation directory `F:/AI/OrcSDR-TEMP/maps-integration-completion/`:

- `maps-picker-navigation-host-build-20261017.log`, `...-host-build-fixed-20261017.log`,
  `...-host-build-final-20261017.log`, `...-setup-build-20261017.log`.
- `maps-picker-navigation-host-test-20261017.log`, `...-host-runtime-test-20261017.log`.
- `maps-picker-navigation-host-build-complete-20261017.log`,
  `maps-picker-navigation-host-complete-20261017.log` (final 4/4 run).
- `maps-picker-navigation-p4-syntax-20261017.log`.

Reviewed final test output and syntax log are beside this report; raw root logs
remain in place. The native syntax helper is private local tooling under `.local`.

## Open gates and next milestone

No device access, reset, flash, broad suite, merge or push in this follow-up.
COM39 remains on previously flashed `6eb22466`; COM17/C6 untouched.
The reported physical Use Location failure is **not yet reproduced or closed on
COM39**. A coordinate-repaint defect is fixed and write verification added, but
neither proves the root cause of the user's observed save failure.

Next: one approved normal native build and COM39 cycle. Compare the same zoom/view
under active LoRa; check the closer start, arrows, physical controls, new button,
save a changed point, verify displayed/stored coordinates and reboot persistence,
then restore the original receiver point. Preserve recovery/NVS/SD and C6; do not
touch COM17. Record the actual build HEAD, binary hash and measured device times.
The coarser-world appearance and speedup require device acceptance.

Previously open saved-capture discrepancies, intermittent SD reads and memory
margins remain open. Regional/dashboard/release gates are not completed by these
host tests. Integration is not ready to claim complete or merged.
