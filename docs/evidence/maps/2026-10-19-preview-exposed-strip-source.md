# Preview exposed-strip clearing and stage timing — October 19, 2026

Status: source implementation and focused host checks only. Not native-built,
flashed or device-verified. No managed-component, engine-pin, RTL-driver,
framebuffer-owner or orientation changes. The deployed COM17 artifact sealed
from `34d3366c` was not modified by this work.

## Behavior

`paint_preview` keeps its existing map/target clip intersection. For an opaque
same-scale snapshot, it computes the intersection covered by the translated
image, using the same rounded displacement as `pushSprite`. It clears only the
exposed complement: top and bottom strips, then left and right strips within
the covered vertical span. Those rectangles do not overlap. A completely
displaced image clears the visible map background. An identity transform needs
no clearing. Scaled previews retain full clipped background clearing.

The adapter continues to use ordinary M5GFX drawing calls and restores the
previous clip. It does not write raw framebuffers, change page flipping or
introduce asynchronous transfers. The existing immutable-snapshot lock and UI
ownership remain in place. The requested 180-degree default is unchanged.

## Measurements added

An optional function-pointer clock, with no platform dependency or allocation,
returns `painted`, `clear_us` and `transfer_us`. An empty clip returns
`painted=false` without clearing, transferring or reading the clock.

The service passes its ESP timer and records `clear_call` and `transfer_call`
alongside the existing total paint, request-feedback and presentation-interval
metrics. Only the first painted preview for a request is recorded. A repaint
retry does not add another sample. Request acceptance timestamps, same-scale
versus scaled bucket semantics, and the dwell/physical-touch disclaimers remain
unchanged. Stage calls measure API execution, not physical panel completion.

Each bucket now has five 256-entry uint64 timing rings, rather than three.
Across both external-BSS buckets, four added rings use 8,192 bytes of sample
storage plus bounded metadata (about 8 KiB additional PSRAM). The focused test
bounds both complete pools to 21 KiB; the former 13 KiB bound is obsolete for
this source. Native linker placement still needs verification in an approved
future build.

## Focused verification

Commands ran from `F:/AI/OrcSDR-TEMP/maps-integration-completion`; logs are saved
in that same directory.

```powershell
cmake --build .local/maps/timing-host-20261019 --config Debug --target orcsdr_map_preview_tests orcsdr_map_timing_tests
ctest --test-dir .local/maps/timing-host-20261019 -C Debug -R '^(map_preview|map_timing)$' --output-on-failure
```

Results: the two focused targets build; CTest reports **2/2 passed**. An initial
successful focused build was followed by a small coverage refinement and the
final focused build. Logs:

- `map-preview-strip-build-20261019-01.log`
- `map-preview-strip-build-20261019-02.log` (final)
- `map-preview-strip-tests-20261019-01.log`

The recording test checks each pixel's clear/opaque-cover count for horizontal,
vertical and diagonal pan directions, positive/negative half-pixel rounding,
identity, maps displaced entirely offscreen, a clip inside the map, a clip partly
overlapping it, empty clips and nonpositive dimensions. Covered pixels are not
cleared; exposed visible pixels are cleared exactly once; pixels outside the
clip are untouched. Scaled paths retain full clipped clearing. Fake-clock
checks verify stage durations and that fully clipped work produces no sample
opportunity. Timing tests check stage duplicate suppression and rolling
256-sample bounds alongside existing percentile/timestamp checks.

`git diff --check` for the five owned source/test files reported no whitespace
errors. No broad host suite or native build ran. Parent verification also ran:

    C:/Espressif/python_env/idf5.5_py3.14_env/Scripts/python.exe .local/maps/check_preview_strip_syntax_20261019.py

P4 map_service syntax passed (exit 0) using ESP-IDF 5.5.4 compile-database
flags. Log: `preview-strip-p4-syntax-20261019.log`. The SDK's existing
literal-suffix warning remains; no link or hardware operation was performed.

## Remaining acceptance

No hardware speed improvement is claimed. The prior completed canvas push alone
was approximately 78 ms, so removing redundant clearing does not prove a 33 ms
preview cadence. A later authorized native build must verify external-BSS size,
screen correctness in all supported orientations, separate clear/transfer
durations, UI response, active-radio continuity and retained-map SD behavior.
The existing copy scout remains a separate historical source assessment.
