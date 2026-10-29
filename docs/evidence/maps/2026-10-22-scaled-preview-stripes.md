# Scaled transfer call amplification and bounded row batching

**Retired after hardware testing.** Candidate `d7541282` passes native build,
artifact, app-only preservation and boot checks, but the same bounded LoRa plan
measures scaled paint p50/p95 76,951/91,354 us versus 57,346/66,722 us in the
preceding `9ab05c04` run. Same-scale painting remains about 23 ms median.
Decoder allocation/load differs, so this is not a controlled speedup ratio;
there is no evidence supporting keeping the added buffer/complexity. The user
also reports LoRa-map flicker. Source row batching and its 40 KiB allocation are
removed, retaining this probe as historical evidence. COM17 app-only rollback to
`9ab05c04` passes previous-app identity, fresh backups, all non-app preservation,
matching ELF startup and LoRa streaming checks. No SD/C6/receiver-setting erase.

Logs: `scaled-stripe-{artifact-inspection,preflash-device,verify-previous,
backup-*,verify-backups,app-write,preservation,postflash-device,controlled-device}-20261022.log`;
native boot in `apps/orcsdr-tab5/scaled-stripe-boot-native-monitor-20261022.log`.
Rollback logs: `lora-flicker-rollback-{preflight-device,verify-previous,backup-*,
verify-backups,app-write,preservation,postflight-device}-20261022.log`;
boot in `apps/orcsdr-tab5/lora-flicker-rollback-boot-native-monitor-20261022.log`.
All successful executions are preserved; the optional ROM-GDB generation warning
does not fail firmware linking. Host call-count reduction did not predict native
performance. The separate unconditional adapter clear and idle LoRa redraw are
addressed in [the follow-up correction](2026-10-22-lora-flicker-and-label-layout.md).

The installed `9ab05c04` candidate measures scaled transfer p50/p95
57,280/66,672 us while LoRa receives, after unnecessary clear cost is removed.
Inspecting pinned M5GFX identifies a separate transfer cost: its affine renderer
submits one logical row per panel call. Tab5's internal rotation 3 transposes each
row into a column. `Panel_FrameBufferBase::writeImage()` then calls its pixel-copy
function once per physical row, effectively once per pixel of the logical row.

## Probe, not a speed claim

`python .local/maps/trace_scaled_panel_calls_20261022.py` executes unchanged
`_rotate_pixelcopy`/`writeImage` bodies from the resolved component with an opaque
counting copy function. It verifies equal pixel totals:

| Image | Full-sprite copy calls | Affine scanline calls | 16-row-strip calls |
| --- | ---: | ---: | ---: |
| Picker 1248×568 | 1,248 | 708,864 | 44,928 |
| LoRa 842×330 | 842 | 277,860 | 17,682 |

The probe exits zero; log `scaled-panel-copy-amplification-20261022.log` in the
integration root. Panel source SHA-256:
`9f4526b98e8a0f7c20aea50df23fd51c8fc18c1f9a66638aabf3a55e98738d67`.
Call-count reduction is not a measured P4 speed multiplier.

## Bounded shared adapter change

`map_scaled_preview.hpp` samples native RGB565 into at most 16 reusable rows,
then `map_service.cpp` uses the public typed image API. The buffer is exactly
40,960 bytes in PSRAM, allocated during completed-frame publication, accounted
in the existing map holder and freed by its existing reclaim path. No task,
framebuffer access, per-tick allocation or internal/DMA buffer is introduced.
All existing dashboards share this adapter. This does not replace basemap
rendering or copy any OrcMaps implementation/API.

Only enlargement factors whose actual M5GFX float value is 2 or 4 are batched.
Shrinking retains the original sparse transfer to avoid copying a full viewport
for an image occupying only a quarter of it. Other factors, unavailable buffers,
unsupported source formats/targets or insufficient scratch capacity retain the
existing renderer. Allocation failure preserves the completed preview.

The sampling bounds deliberately derive from pinned M5GFX/LovyanGFX fixed-point
pixel-centre math. Credit and upstream notices are retained in
`docs/licenses/{LovyanGFX-license.txt,M5GFX-LICENSE.txt}`; the upstream LovyanGFX
notice is fetched from the URL linked by the component's own header. Notice SHA:
`ec7c22d763503f016c1e743e6089042e780fbb1c118ebcc0b1eb72232e8903b3`.
No managed component is edited or forked.

Read-only diagnostics append `ORC_MAP_PREVIEW_BUFFER` with bytes, rows and paint
count; per-frame facts append `stripe_backend`. Transfer timing includes sampling
and public image pushes, not just bus activity. See `docs/MAP_SERIAL_COMMANDS.md`.

## Focused validation

From `F:/AI/OrcSDR-TEMP/maps-integration-completion`:

```text
wsl -d Ubuntu -- bash -lc 'cd /mnt/f/AI/OrcSDR-TEMP/maps-integration-completion && cmake --build .local/maps/native-preview-host-20261022 --target orcsdr_map_preview_native_tests orcsdr_map_preview_tests orcsdr_map_preview_format_tests -j2 && ctest --test-dir .local/maps/native-preview-host-20261022 -R "^map_preview(_native|_format)?$" --output-on-failure && python3 tests/map_preview_memory_test.py'
wsl -d Ubuntu -- bash -lc 'cd /mnt/f/AI/OrcSDR-TEMP/maps-integration-completion && cmake --build .local/maps/native-preview-asan-20261022 --target orcsdr_map_preview_native_tests -j2 && ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir .local/maps/native-preview-asan-20261022 -R "^map_preview_native$" --output-on-failure'
python .local/maps/check_picker_navigation_syntax.py
```

The new conformance test initially fails because the sampler header/API does not
exist (`scaled-stripe-native-red-20261022.log`). The old service also fails the
new allocation/accounting expectation (`scaled-stripe-memory-red-20261022.log`).
Current tests compare actual output colours with unchanged pinned raster bodies,
including picker/LoRa/ADS-B/odd-sized images, fractional offsets, rounding to float,
clipping, scratch canaries, unsupported factors and capacity failure. The fault
test extracts production publication/held/release functions and injects allocation
failure, retry, lock contention and low-memory fallback under ASan/UBSan.

Actual results: normal preview/raster/format 3/3 PASS, raster ASan/UBSan 1/1 PASS,
memory fault injection PASS, five P4 syntax checks PASS (existing SDK warnings).
The raster run covers 1,440 combinations, including 720 exact enlarged-image
comparisons and unsupported-factor fallbacks. Logs in the integration root:
`scaled-stripe-{host-final2,asan-final2,p4-syntax-final3,memory-fault}-20261022.log`.

Native linking and hardware performance, reclaim/SD/radio, stack and colour gates
remain open. COM17 remains on `9ab05c04`; COM39 is untouched. Overlapping node
names and the overall integration/performance acceptance remain separate work.
