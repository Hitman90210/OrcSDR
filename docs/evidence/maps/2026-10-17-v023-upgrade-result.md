# OrcMaps v0.2.3 dependency upgrade: build and host verification

Date: 2026-10-17, America/Los_Angeles.
OrcSDR source commit: `99affb1b4463b8438a0a3e2b939406e7434659a4`.
Engine: `dc6bc12b7df9fb3decd751709da733a386cf386e`, version 0.2.3.
Hardware was not accessed in this cycle. This report does not qualify device speed.

## Source and release checks

Verified published refs from the OrcMaps remote:
main and peeled v0.2.3 point at dc6bc12; annotated tag object is
`2dc5079f29edba65ad62f5b235120595e3d31bb5`.
`git merge-base --is-ancestor c3cc09de dc6bc12` exits 0.
The fetched manifest reads 0.2.3, polygon-ring rendering is retained, and cleanup
still swaps out feature-byte and layer vectors without allocating.

The earlier e3028c60 and c3cc09de lines diverged; e3028c60 did not historically
revert c3cc09de. The earlier report described the effect of replacing the pin,
not a chronological reversal. The v0.2.3 descendant resolves those two blockers.

Only two production configuration files changed in this upgrade:

- `apps/orcsdr-tab5/main/idf_component.yml`: exact v0.2.3 commit pin and comment.
- `apps/orcsdr-tab5/dependencies.lock`: regenerated OrcMaps revision/hash and
  manifest hash. All 11 other dependency records compare exactly equal.

No OrcSDR renderer/application source change, vendoring, copied engine,
RenderScratch adoption or workaround. The current five-argument RenderFeatureAt
call compiles. No OrcSDR-owned TileScreenMap definition/manual Q16 field filling
was found in the targeted source search. Existing dependency and miniz gates remain.

The separate larger-footer commit `73f84ce7` is included in this candidate. Its
appearance has not been checked on hardware. Do not confuse identical basemap
pixels with intentionally changed footer typography.

## Component resolution

The initial wrapper invocation stopped during configuration with the gate's
requested-dc6bc12/locked-c3cc09de mismatch. It did not produce the new firmware.
The component manager retained the old Git lock despite the manifest change.

The stale managed OrcMaps directory was moved intact to private
`.local/maps/orcmaps-c3-managed-backup/`, after checking source and destination
paths. No managed source was hand-edited. The installed component-manager
LockManager invalidated only the OrcMaps solved entry, then its dependency
resolver regenerated/downloaded the solution. An import typo in the private
helper was corrected before resolution. No component-manager source was edited.

Requested/locked/fetched agreement now PASS. Component hash:
`cd07da117d4f084e09286956649334d9a649fb5e9ad16a087e380d02c4e10dfb`.
The untouched other dependency records were checked before compiling.
No whole-lock dependency refresh or unrelated version advance was accepted.

## Pixel and host results

A private standalone probe links OrcSDR's unchanged map_pipeline.cpp against each
engine and the engine's host framebuffer. It uses the same verified Natural Earth
world archive on both sides. Baseline was built from the unchanged c3cc09de
sibling checkout before the pin changed; candidate uses the fetched managed
v0.2.3 component. No engine code is copied into the probe.

36 comparisons: dimensions 900x330 and 1248x568; latitude 44.05; longitudes
-123.022, 0 and 179.99; requested zooms 0, 2, 3, 4, 7 and 8. Both builds use the
same existing world-overview source-level policy. Every sampled render reports
missing=0/failed=0; all 36 complete PPM outputs compare **byte-identical**, not
just by a visual check. Both hashes for every output are in the companion JSON.

Commands from `F:/AI/OrcSDR-TEMP/maps-integration-completion/`:

```powershell
cmake -S apps/orcsdr-tab5/tests/maps -B .local/maps/pipeline-host -DCMAKE_PROJECT_INCLUDE=F:/AI/OrcSDR-TEMP/maps-integration-completion/.local/maps/v023-pixel-hook.cmake
cmake --build .local/maps/pipeline-host --config Release --target orcsdr_pixel_probe
# Run before changing ORCMAPS_ROOT: baseline c3cc09de checkout.
.local/maps/pipeline-host/Release/orcsdr_pixel_probe.exe .local/maps/world/world-overview-z4.pmtiles .local/maps/pixel-v023/baseline
cmake -S apps/orcsdr-tab5/tests/maps -B .local/maps/pipeline-host -DORCMAPS_ROOT=F:/AI/OrcSDR-TEMP/maps-integration-completion/apps/orcsdr-tab5/managed_components/orcmaps
cmake --build .local/maps/pipeline-host --config Release --target orcsdr_pixel_probe orcsdr_maps_tests orcsdr_location_store_tests orcsdr_setup_tests
.local/maps/pipeline-host/Release/orcsdr_pixel_probe.exe .local/maps/world/world-overview-z4.pmtiles .local/maps/pixel-v023/candidate
python .local/maps/compare_v023_pixels.py
ctest --test-dir .local/maps/pipeline-host -C Release -R '^(location_store|setup_flow|map_pipeline|world_geometry)$' --output-on-failure
```

MSVC sanitizer DLL directory was prepended to PATH for host runs:
`C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC/14.50.35717/bin/Hostx64/x64`.
Result: pixel comparison PASS; four targeted CTest tests PASS, 1.25 s. Existing
ASAN build configuration was retained, no sanitizer finding in this bounded run.
The new engine's full upstream 179-test suite was reported by the upstream author,
not rerun here. These integration checks do not prove all map packs or device
RGB565/M5GFX output; a COM39 comparison remains pending.

## Native result

After successful pixel/host checks, the pin/lock were committed and the wrapper
completed from source 99affb1b:

```powershell
& ./apps/orcsdr-tab5/tools/build-tab5-idf.ps1 -WorldMap F:/AI/OrcSDR-TEMP/maps-integration-completion/.local/maps/world/world-overview-z4.pmtiles
```

PASS: configure, compile/link, requested/locked/fetched gate, linked miniz gate
(two bundled symbols, none resolving to ROM) and partition size check.
Normal diagnostics OFF; full internal DMA reserve 40,960 bytes unchanged.
App 5,028,464 bytes; factory headroom 1,262,992 bytes (20%). SHA-256:
`a862da09eb48d695c2653ddb8462784a35ccf88ead06179a0a15cc289b20f447`.
ELF SHA-256:
`91b577c0e6c9532bce550894eb1d7ca1c5ca0f5706a5266add586b14e5585016`.
Binary/ELF preserved privately as `.local/maps/v023-candidate.bin/.elf`.
Existing compiler warnings remain; this is not a warning-free build claim.

## Logs, limitations and next milestone

Logs saved in the invocation directory:

- `maps-v023-native-build-20261017.log`: initial stale-lock configuration failure.
- `maps-v023-resolution-20261017.log`, `maps-v023-resolution-fixed-20261017.log`.
- `maps-v023-baseline-{config,build,pixels}-20261017.log`.
- `maps-v023-host-{config,build,test}-20261017.log`.
- `maps-v023-candidate-pixels-20261017.log`.
- `maps-v023-pixel-comparison-20261017.log/.json`.
- `maps-v023-dependency-verification-20261017.log`.
- `maps-v023-resolved-native-build-20261017.log`: successful native build.
- `maps-v023-build-manifest-20261017.json`.

Reviewed build-summary, host, pixel and dependency records are beside this report.
Raw images/probe/helper files remain under `.local/maps/`; they are not installed
in firmware. Prior recovery/device artifacts remain preserved.

COM17, COM39, C6, NVS, SD and world partitions were untouched. Last flashed COM39
firmware is still 04898891, not this candidate. No push, PR merge or release.

Next milestone is separately approved COM39 flash, boot/footer visibility,
identical-view map comparison and active-LoRa redraw/radio/memory measurements.
No ESP32 speed multiplier, new heap margin or runtime improvement is claimed.
Existing map/SD/memory and remaining regional/dashboard/setup/release acceptance
gates remain open. Integration is not complete or merged.
