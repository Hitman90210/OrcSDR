# Map preview copy performance scout — October 19, 2026

Scope: source inspection only. No renderer changes, native builds, hardware access,
network operations or managed-component edits were performed for this scout. This
note is not approval to implement a DMA shortcut.

## Observed costs supplied by the parent hardware run

COM17 measurements, with the immutable map snapshot retained:

| Operation | Samples | Median | p95 |
| --- | ---: | ---: | ---: |
| Same-scale preview paint | 19 | 116.189 ms | 121.629 ms |
| Scaled preview paint | 3 | 246.232 ms | 251.991 ms |
| Completed canvas push | Not supplied here | Approximately 77.8 ms | Not supplied here |

These paint-call measurements do not establish physical touch latency or separate
clear, rotation, scaling and transfer costs. Three scaled samples are insufficient
for a broad performance claim. The parent also reports an 8 KiB scratch-file
write/readback hash check passing with both map frames retained. A subsequent
Home command encountered expired automation authentication; that is not evidence
of a firmware crash.

## Verified implementation paths

- `apps/orcsdr-tab5/ui/map_preview.hpp::paint_preview` intersects the map area with
  the current graphics clip, clears that whole intersection, then calls
  `pushSprite` for unit scale or `pushRotateZoom` for scaling. It restores the clip.
- `managed_components/m5stack__m5gfx/src/lgfx/v1/panel/Panel_FrameBufferBase.cpp`
  implements a row-`memcpy` fast path in `writeImage` only when internal rotation
  is zero, pixels are opaque and no conversion is required. Other cases use the
  pixel-copy path. Actual internal rotation on the measured device remains
  unverified; this source condition does not prove the measured cost breakdown.
- The public `copyRect` operation copies within the existing framebuffer, using
  an `alloca` row buffer and two copies. It is not an external immutable-snapshot
  block-copy API.
- `tools/patches/m5gfx-tab5-pageflip.patch` adds two display framebuffers and the
  `Panel_DSI::getFrameBuffer` / `getPanelHandle` accessors. Neither accessor grants
  ownership or provides a synchronization primitive. The apply script checks
  patch compatibility; managed files must not be hand-edited.

## Exact SDK check: ESP-IDF 5.5.4

The parent identifies the actual build SDK as
`C:/Espressif/frameworks/esp-idf-v5.5.4`. This scout inspected that exact SDK's
`components/esp_lcd/dsi/esp_lcd_panel_dpi.c` and
`components/esp_lcd/dsi/include/esp_lcd_mipi_dsi.h`.

`esp_lcd_panel_draw_bitmap` reaches `dpi_panel_draw_bitmap`. For an external
compact image, this copies into the currently selected physical framebuffer.
The installed M5GFX `Panel_DSI::init_dpi` enables `use_dma2d` for IDF 5.x.

The DMA2D branch:

- Takes `draw_sem` without waiting and rejects overlapping copies with
  `ESP_ERR_INVALID_STATE`.
- Writes the source cache back before DMA. The source is a contiguous rectangle
  with no independent source stride in this API.
- Queues `esp_async_fbcpy`; a successful return does not mean the source is free
  for reuse or that the pixels have reached the physical screen.
- Releases its semaphore from `async_fbcpy_done_cb`, then calls
  `on_color_trans_done`. That callback marks completion of the source-buffer
  copy, separately from `on_refresh_done` for screen refresh.

When the supplied pointer lies inside a display framebuffer, the function takes
a different path: it flushes the relevant framebuffer cache and selects that
framebuffer. It does not treat the pointer as an arbitrary compact source tile.

Consequently, a direct call is not a drop-in replacement for `pushSprite`:
logical-to-physical rotation, clipping, crop stride, RGB565 representation,
selected front/back buffer, asynchronous source lifetime and shared display
ownership must all be resolved. Existing `rf_visualizer.cpp` framebuffer and
page-flip ownership must remain intact. Graphics `startWrite`/`endWrite`, the raw
pointer accessors and the SDK copy semaphore do not together establish safe
cross-owner coordination.

## Minimum next profiling / optimization seam

1. Instrument clear and image-transfer durations separately at `paint_preview`,
   retaining bounded PSRAM recordings and distinguishing same-scale from scaled
   transforms. Record visible clipped area and actual internal rotation alongside
   the measurements. Keep the existing snapshot lock and UI ownership.
2. For same-scale opaque previews, consider clearing only exposed strips outside
   the translated image intersection. The image already overwrites the covered
   pixels. Verify clipping, offscreen images, positive/negative shifts and the
   full-background case using the recording graphics test before any native run.
   The benefit remains unmeasured.
3. Reassess transfer cost after the split measurement. A direct DMA path requires
   a separate coordinated ownership/completion design and targeted tests; source
   inspection alone does not justify implementing it or predicting its speed.

The user's fresh-install 180-degree orientation requirement remains unchanged.
Changing orientation is not a proposed performance fix. Preserve framebuffer
protections, immutable PSRAM snapshots, the RTL driver and existing screen owners.
