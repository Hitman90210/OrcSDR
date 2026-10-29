# Detailed-pan reuse feasibility — October 23, 2026

Result: do not ship a shifted-frame-only detail completion or an unbounded
decoded-tile cache. These host probes do not close the COM17 two-second refill
gate and make no hardware speed claim. No firmware or device state changed.

## Shifted overlap is not a complete redraw

Compared the existing shared Pipeline's full render at the old camera with a
full render after an integer pan. Labels were disabled. Compared only pixels
present in both views after applying MakeFrameTransform; scale was exactly one
and translation was within 1e-6 of an integer. All assertions for archive open,
viewport selection and full renders succeeded.

Archive: existing global-z8-candidate.pmtiles, engine working head 7125ec1;
OrcSDR Pipeline source unchanged from 30a00ce1. Dimensions 842 by 330.
The zoom-9 view uses stored zoom-8 tiles through pack_source_view.

| Center / requested zoom | Pan 40,0 | Pan 0,20 | Pan -40,0 | Pan 0,-20 |
|---|---:|---:|---:|---:|
| 44.0628866,-123.0160273 / 9 | 7 | 7 | 29 | 8 |
| 35.7,139.7 / 8 | 2 | 7 | 1 | 12 |
| 0,179.99 / 5 | 0 | 0 | 0 | 0 |

Values are mismatching overlap pixels, from 261,020–264,660 comparisons per pan.
Eight of twelve cases differ. The localization log records sample coordinates
and bounding rectangles. Some differences are at old/new viewport edges; this
does not prove that repainting a fixed edge band repairs every case. No cause
is established by this comparison alone. Label-on layout was not tested.

Command, from the integration root:

```text
wsl -d Ubuntu -- bash .local/maps/run_pan_overlap_probe_20261023.sh > maps-pan-overlap-localization-20261023.log 2>&1
```

The executable returns 2 deliberately when any overlap differs. The outer
PowerShell/WSL invocation reported exit 1; inspect the actual result marker,
PAN_OVERLAP_RESULT mismatched_views=8, rather than interpreting that wrapper
status as a compilation failure. Initial comparison log:
maps-pan-overlap-probe-20261023.log. Localization source:
.local/maps/pan_overlap_probe_20261023.cpp, SHA-256
530e8bc69f90575316155886da06f3b6a958e7c2659d35b015870b39b53baf4c.

## Whole decoded geometry does not fit a safe default cache

Streamed the same six unique tiles through the existing no-text layer filter
and protomaps-v4 classifier, counting classified geometry without retaining it.
5,746 classified features contain 7,714 paths and 188,677 points. Point
coordinates alone require 1,509,416 bytes at sizeof(Point)=8. This excludes
path/feature containers, properties, allocation overhead, labels and cache index.
The restored COM17 sample had 1,698,828 bytes free PSRAM; these quantities do not
establish safe cache headroom. Counts are a host observation of this fixed view,
not a global maximum or a measured P4 cache allocation.

The first shell wrapper completed the probe but then failed on a trailing CRLF
line (command not found). Corrected that local wrapper to LF, then ran the
already-built executable directly; exit 0, without recompilation:

```text
wsl -d Ubuntu -- bash -c 'cd /mnt/f/AI/OrcSDR-TEMP/maps-integration-completion && .local/maps/decoded_tile_footprint_20261023 /mnt/f/AI/OrcSDR-TEMP/orcmaps-quality-delivery/.local/global-assessment/global-z8-candidate.pmtiles' > maps-decoded-tile-footprint-rerun-20261023.log 2>&1
```

Preserved first log: maps-decoded-tile-footprint-20261023.log. Source:
.local/maps/decoded_tile_footprint_20261023.cpp, SHA-256
dd08198900391b74f440db031e2248bac62943bdc1a15fcff16975cc23656f2e.
All logs and exploratory sources remain in the invocation worktree.

## Next performance gate

Choose a bounded reduction in repeated decode/draw work. Any retained geometry
or drawing-command representation needs an explicit byte cap, allocation-failure
fallback, source/camera identity and reclaim behavior. Any raster reuse needs
full-output equivalence across viewport edges and label changes. Prove those
properties before another native candidate, then measure the unchanged COM17
20-pan workload under reception. Current accepted firmware stays 30a00ce1;
p95 4.552282 seconds still fails the required two seconds.

## Compact geometry replay follow-up

A host-only prototype delta-encodes classified tile-local geometry into varints,
then reconstructs Features and calls the existing RenderOrderedFeatureAt and
FinishOrderedTile for each new camera. It redraws geometry at the new camera;
it does not shift the old raster or introduce a replacement rasterizer.
Source snapshot: [prototype](probes/2026-10-23-feature-replay.cpp), SHA-256
a797cc01ed08577cc77c8629e9345cddf4971c8c8d02db8549d5ddbf9f026d96.

| View | Full-frame comparisons | Result | Encoded payload | Sum of vector capacities |
|---|---:|---|---:|---:|
| Reference zoom 9/source 8 | 5 | Zero differing pixels | 433,391 B | 573,440 B |
| Antimeridian zoom 5 | 5 | Zero differing pixels | 4,547 B | 5,280 B |
| Tokyo zoom 8 | 0 | Capacity miss before comparison | At least 634,309 B | Not accepted |

Each passing group compares the starting view and independent pans 40,0;
0,20; -40,0; 0,-20. Comparisons cover all 277,860 pixels, labels disabled.
Synthetic checks reject every truncation of a small encoded polygon, an invalid
kind and an impossible path count. These are focused prototype checks, not a
production decoder validation suite. Feature properties and label data are not
stored; this is not an implementation ready to replace the shared Pipeline.

The reference payload fits 512 KiB, but its vector allocations do not. A real
cache must use explicitly capped storage rather than accepting payload size as
allocation size. Dense views need normal streaming fallback and eviction; the
prototype deliberately reports a capacity miss instead of implementing either.
Label-on reuse, allocation failures, cancellation, cache identity/invalidation,
reclaim, P4 placement and hardware timing remain untested.

Commands from the integration root:

```text
wsl -d Ubuntu -- bash .local/maps/run_feature_replay_probe_20261023.sh > maps-feature-replay-probe-expanded-20261023.log 2>&1
wsl -d Ubuntu -- bash -c 'cd /mnt/f/AI/OrcSDR-TEMP/maps-integration-completion && .local/maps/feature_replay_probe_20261023 /mnt/f/AI/OrcSDR-TEMP/orcmaps-quality-delivery/.local/global-assessment/global-z8-candidate.pmtiles 35.7 139.7 8' > maps-feature-replay-tokyo-20261023.log 2>&1
wsl -d Ubuntu -- bash -c 'cd /mnt/f/AI/OrcSDR-TEMP/maps-integration-completion && .local/maps/feature_replay_probe_20261023 /mnt/f/AI/OrcSDR-TEMP/orcmaps-quality-delivery/.local/global-assessment/global-z8-candidate.pmtiles 0 179.99 5' > maps-feature-replay-antimeridian-20261023.log 2>&1
```

First and third invocations exit 0. Tokyo's executable intentionally returns 3
for capacity miss; the PowerShell/WSL wrapper reports exit 1, with the explicit
REPLAY_CAPACITY_MISS marker in its log. An earlier prototype's invalid-count
self-test changed a byte in the extent instead of the path count and failed;
maps-feature-replay-probe-20261023.log preserves that harness mistake. Corrected
the byte index and reran before obtaining these results. No firmware flashed,
no device accessed, no speed claim. The next implementation candidate is bounded
geometry replay with preserved label behavior and streaming fallback; it still
must pass the unchanged native refill gate.
