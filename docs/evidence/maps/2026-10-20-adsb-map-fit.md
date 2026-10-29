# ADS-B map fit source correction — October 20, 2026

The user clarified that the map does not fit its allocated dashboard slot well
and asked to leave COM17 available. No device access was performed.

`apps/orcsdr-tab5/ui/adsb_dashboard.cpp` previously requested a 360×360 square
inside a 646×390 panel, with radar-style cardinal positions and a separately
calculated receiver center. The candidate requests a 622×302 rectangle at
panel-relative (12,76), leaving a heading strip and consistent margins.
Global map bounds are (246,164) through the half-open edge (868,466).

The receiver marker uses this rectangle's center, and aircraft projection still
uses the same shared RegionalView as the basemap. Overlay clipping prevents
aircraft labels from spilling into the heading or neighboring panels. The old
four-sided compass arrangement is replaced by a small north indicator.
No radar rings or crosshair were added.

Zoom buttons are 48×48 with shared drawing/touch constants, handled before
aircraft hit testing. Plus reduces range and minus increases it, clamping at
10 and 100 NM instead of wrapping. Compile-time checks cover endpoints and
the 25→10 / 25→50 steps. Existing Settings range cycling remains unchanged.

Executed from `F:/AI/OrcSDR-TEMP/maps-integration-completion`:

```text
python .local/maps/check_paint_order_adsb_20261020.py
```

Result: **4/4 P4 syntax checks pass**, including ADS-B. The script uses the real
saved P4 compilation flags and local candidate-engine include paths; it does
not link or change the pinned component. Log:
`paint-order-adsb-p4-syntax-20261020.log` in the invocation directory.
A narrow read-only review found no additional concrete layout/hitbox defect.

The same session prepared a shared ordered-rendering adapter in
`map_pipeline.cpp/.hpp`, `map_service.cpp` and `map_pipeline_tests.cpp`.
At this standalone layout checkpoint those adapter changes were uncommitted;
the subsequent [ordered integration checkpoint](2026-10-20-ordered-rendering-integration.md)
records their pin/lock resolution and source integration. Engine evidence:
`F:/AI/OrcSDR-TEMP/orcmaps-quality-delivery/docs/evidence/2026-10-20-ordered-tile-rendering.md`.

Native linking, real button interaction, physical fit/readability, aircraft
alignment and RF/memory behavior remain unverified for the new rectangle.
The larger map request can increase worker framebuffer memory even though the
646×390 dashboard sprite size is unchanged. COM17 still runs the prior FT8
candidate, and COM39 is untouched. Do not claim the new layout is installed.
