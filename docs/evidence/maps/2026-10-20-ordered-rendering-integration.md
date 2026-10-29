# Ordered OrcMaps integration — October 20, 2026

## Source and dependency checkpoint

OrcMaps `b59d47819cc3b81c1bbe50ed75b775275bb0218e` passed the approved
complete configured host regression: **8/8 CTest tests**, zero failures,
0.23 seconds. The eight prepared engine commits were pushed to
`codex/map-quality-delivery`, and `git ls-remote` confirmed that exact SHA.
No main merge, tag, release or map-data publication occurred.

Engine invocation directory: `F:/AI/OrcSDR-TEMP/orcmaps-quality-delivery`.

```sh
cmake --build .local/boundary-host-20261019 -j2
ctest --test-dir .local/boundary-host-20261019 --output-on-failure
```

Logs: `ordered-full-host-build-b59d478-20261020.log` and
`ordered-full-host-tests-b59d478-20261020.log` in that directory.
This is the complete configured CMake set, not every optional Python/font/tool
test in the repository. Earlier focused sanitizer/allocation-failure and
worldwide comparison evidence is in the engine's
`docs/evidence/2026-10-20-ordered-tile-rendering.md`.

OrcSDR changes:

- `main/idf_component.yml` and regenerated `dependencies.lock` pin that revision.
- `map_pipeline.cpp/.hpp` use the engine's bounded ordered queue. One queue is
  reused across source tiles and frames, with discard/reset on failure and
  cancellation. Polygon workspace retains its former per-tile lifetime.
- `map_service.cpp` exposes queue peak/retained bytes and capacity failures via
  `ORC_MAP_DEFERRED`; an oversized tile is reported explicitly. Failed or
  cancelled frames are not presented as successfully rendered maps.
- `tests/maps/map_pipeline_tests.cpp` checks eight real global viewports when
  the existing archive environment variable is supplied, with optional PPMs.
  Existing geometry/label/save/return/cancellation checks still run.

The independent ADS-B map-fit source is committed at `78e0815c` and is described
in [its evidence note](2026-10-20-adsb-map-fit.md).

## Dependency resolution safeguards

The first explicit reconfigure retained the old git lock entry. The existing
requested/locked/fetched gate rejected it before any firmware compilation.
The official `idf.py update-dependencies` then resolved the new pin but also
offered unrelated Wi-Fi Remote 1.6.5 and mDNS 1.14.0 updates. Those were rejected.
The manifest now explicitly preserves the already-used versions 1.6.4 and
1.13.1, respectively. The corrected official refresh completed successfully.

A parsed comparison against the preserved pre-change lock verifies **only the
OrcMaps dependency record changed**. All other resolved component records and
the direct-dependency list are identical. No generated lock or OrcMaps managed
source was edited by hand. Existing standard M5GFX pageflip and ESP-Hosted
trampoline patch scripts were applied idempotently after resolution.

Commands from `F:/AI/OrcSDR-TEMP/maps-integration-completion`:

```text
python .local/maps/build_fit_order_20261020.py reconfigure
python .local/maps/build_fit_order_20261020.py update-dependencies
python .local/maps/build_fit_order_20261020.py update-dependencies scoped-dependencies
```

The first command failed its pin gate; the second succeeded but produced the
rejected unrelated version drift; the third succeeded with scoped dependencies.
Each command has its own `fit-order-native-*-20261020.log`. The parsed comparison
is saved in `fit-order-dependency-scope-20261020.log`.

## Host and syntax evidence

Actual integration Pipeline, linked to the reviewed engine checkout, passes
its bounded executable with the verified global archive. Eight zoom-8,
1280×720 geometry frames find all 24 tiles, report zero capacity failures,
and match corrected batch output byte-for-byte. Latest reviewed-source run:

```sh
cmake --build .local/maps/labels-host --target orcsdr_maps_tests -j2
ORCSDR_ORDER_PREVIEW_DIR=.local/maps/paint-order-previews-20261020 ORCSDR_LABEL_TEST_ARCHIVE=/mnt/f/AI/OrcSDR-TEMP/orcmaps-quality-delivery/.local/global-assessment/global-z8-candidate.pmtiles .local/maps/labels-host/orcsdr_maps_tests /mnt/f/AI/OrcSDR-TEMP/orcmaps-quality-delivery/tests/fixtures/tiny-gzip.pmtiles /mnt/f/AI/OrcSDR-TEMP/orcmaps-quality-delivery/examples/m5stack-tab5/test-pack/springfield-97477.manifest.json
```

Logs: `paint-order-pipeline-reviewed-build-20261020.log`,
`paint-order-pipeline-reviewed-tests-20261020.log`, and
`paint-order-world-parity-20261020.log`, all in the invocation directory.
Queue peak reaches 333,456 bytes; retained capacity reaches 522,240 bytes on
the 64-bit host. Those are not P4 memory measurements.
Four actual-P4 syntax checks pass: engine renderer, pipeline, service and ADS-B;
log `paint-order-adsb-p4-syntax-20261020.log`.

## Native and device gates

The combined native build is the next step. Use explicit saved SDKCONFIG,
ESP_IDF_VERSION=5.5, the existing C6 3.0.6 image, diagnostics OFF, unchanged
40 KiB internal reserve and existing symbol gates. Preserve artifact/configuration
provenance under `.local/maps/fit-order-b59d478-20261020/`.

No device was accessed during these changes. COM17 is left available at the
user's request; it still runs the prior FT8 candidate. COM39 belongs to Dead Air
and is untouched. Native link, P4 placement/memory, ADS-B physical fit and
aircraft alignment, retained-map SD and active-radio checks remain open.
The full map integration goal is not complete.
