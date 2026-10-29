# Streamed place-label integration — host and syntax evidence

October 18, 2026. Source preparation only; no device changes.

## Dependency and implementation

User approved pushing the OrcMaps development branch. Remote
`refs/heads/codex/map-quality-delivery` was verified as
`5f3a1eb767b26aa7df00158308f40331b298d7ef`. This is not a main merge or release.
OrcSDR's manifest now pins that revision. The component manager regenerated
`dependencies.lock`; all unrelated dependency entries compare unchanged.
Log: `label-pin-resolution-sdk-20261018.log`. Earlier attempts failed before
resolution due to a resolver-script assertion and the wrong Python environment;
the successful run used the Python 3.14 environment in the build cache.
No managed component source was hand-edited.

The shared map pipeline stores a supported static profile name and uses the
engine's profile-aware streaming classifier. Optional labels retain place
layers, copy names before callbacks return, use the same camera/placement as
geometry, and draw after all tiles. Geometry reuses one RenderScratch per tile.
Existing no-label callers remain supported. A label layout without its text
target, or vice versa, is rejected.

The map service owns a fixed LabelLayout in `EXT_RAM_BSS_ATTR` and uses the
existing DejaVu24 font on its private canvas. Missing names are counted in
`ORC_MAP_LABELS` diagnostics and reported in frame status. Physical glyph
coverage and actual PSRAM placement/link cost are not yet verified.

## Tests actually run

From the integration worktree, under WSL:

```
cmake -S apps/orcsdr-tab5/tests/maps -B .local/maps/labels-host -DORCMAPS_ROOT=/mnt/f/AI/OrcSDR-TEMP/orcmaps-quality-delivery
cmake --build .local/maps/labels-host --target orcsdr_maps_tests -j4
ctest --test-dir .local/maps/labels-host -R '^map_pipeline$' --output-on-failure
```

The initial compile failed because LabelLayout/the new render arguments were
absent. After implementation: **1/1 passed**, including existing map-screen,
pipeline repeatability, allocation-failure and cancellation checks. Logs:
`label-pipeline-config-20261018.log`, `label-pipeline-red-20261018.log`,
`label-pipeline-green-build-20261018.log`, `label-pipeline-green-20261018.log`.

```
ORCSDR_LABEL_TEST_ARCHIVE=/mnt/f/AI/OrcSDR-TEMP/orcmaps-quality-delivery/.local/global-assessment/global-z8-candidate.pmtiles ctest --test-dir .local/maps/labels-host -R '^map_pipeline$' -V
```

**1/1 passed** against the actual global candidate. Checks include streaming
Eugene labels, repeatable names and pixels, then moving to London with London
present and Eugene absent. London: 443 offers, 47 drawn with the deterministic
test text metrics. These are pipeline checks, not real-font quality claims.
Logs: `label-pipeline-global-build-20261018.log`, `label-pipeline-global-20261018.log`.
Real-font image checks are documented separately in the OrcMaps worktree.

`python .local/maps/check_label_pipeline.py`: **two syntax checks passed**
against real cached P4 SDK flags, covering map_service.cpp and map_pipeline.cpp.
Log: `label-pipeline-syntax-20261018.log`. No native firmware link, build or flash.

## Remaining gates and next milestone

Native linking and symbol/placement checks, then approved hardware validation
of memory, font readability, label diagnostics and radio continuity. The global
profile is not yet enabled through pack validation/discovery. Worldwide style
and boundary acceptance, cached interaction, catalog/download and complete
setup remain open. Preserved wizard edits are separate uncommitted work.
