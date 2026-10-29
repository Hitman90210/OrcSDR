# Preview panel-facts diagnostic — October 19, 2026

Status: source-only diagnostic, one ESP32-P4 syntax check passed. Not linked,
flashed or observed on hardware. No runtime rotation or copy-path result is
claimed. The previously sealed/flashed location candidate is unchanged.

## Change

Only `apps/orcsdr-tab5/ui/map_service.cpp` was changed for this diagnostic.
After an actual preview paint, it collects one latest fact record per existing
same-scale/scaled bucket while holding the existing preview lock. Reports copy
the records under that lock and format/write them after releasing it. The two
records are stored in external BSS and statically bounded to 256 bytes total;
there is no allocation, new timing ring or new lock.

`ORC_MAP_PREVIEW_FACTS` reports:

- Request serial and whether the target is the physical `M5.Display`.
- Raw panel rotation and configured rotation offset, with explicitly derived
  internal rotation.
- Physical panel dimensions, logical target dimensions and snapshot dimensions.
- Snapshot/target color-depth enum values, the restored target clip rectangle,
  clipped map area and clipped image bounding-box area.

The fact record is the latest observed paint, not a percentile-associated
sample. Existing timing rings retain their previous first-request behavior.
Collection happens after the recorded paint finish timestamp, so its work is
not included in the existing paint/clear/transfer call durations.

## Supported APIs and derivation

The checked M5GFX headers expose `M5.Display.getPanel()`, panel `getRotation()`,
panel `config()`, target `width()` / `height()`, `getColorDepth()` and
`getClipRect()`. The diagnostic reads `config().offset_rotation`, `panel_width`
and `panel_height` only when the actual target is `M5.Display`; it does not
reinterpret an arbitrary sprite target's panel as a physical panel.

The exact formula in the checked
`Panel_FrameBufferBase.cpp::setRotation` is:

```cpp
((rotation + offset) & 3) | ((rotation & 4) ^ (offset & 4))
```

The output names this `derived_internal_rotation`; no public getter for the
protected internal field was invented. Raw rotation alone is not assumed to be
internal rotation. Actual runtime values remain unverified.

Same-scale copy area uses the same rounded translation as `pushSprite` and the
intersection of source, map and restored target clip. `copy_bbox_exact=1`
identifies that geometric covered area. For scaled previews the value is an
estimated geometric bounding box (`copy_bbox_exact=0`), using outward floor/ceil;
it is not a measured M5GFX affine raster pixel count. Color-depth enum equality
is not presented as proof of a particular internal pixel-copy fast path.

This does not change rotation, clipping, pixels, framebuffer selection, UI
ownership, DMA behavior, callbacks or the default 180-degree orientation. It
does not expose or write a raw framebuffer.

## Syntax check

From `F:/AI/OrcSDR-TEMP/maps-integration-completion`, load the existing entry for
`ui/map_service.cpp` in
`apps/orcsdr-tab5/build-native-hosted3/compile_commands.json`; preserve its
compiler/options, remove output/dependency-generation options and invoke it once
with `-fsyntax-only` for the current source in the cached build directory.

Result: **exit 0**, using the cached ESP-IDF
`C:/Espressif/frameworks/esp-idf-v5.5.4` includes. The output contains an existing
SDK `periph_ctrl.h` literal-suffix warning. No native link or test suite ran.

Saved log in the invoking worktree root:
`preview-panel-facts-p4-syntax-20261019-01.log`.

The next authorized native/device cycle must confirm the logged facts during
same-scale and scaled previews before choosing a transfer optimization. This
source change makes no 33 ms, DMA speed or physical input-latency claim.
