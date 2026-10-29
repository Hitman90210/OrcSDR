# Picker blackout during movement — source correction

User physically confirmed corrected drag/tap behavior on COM17, then reported
black map pixels during dragging and tap-to-recenter. This is a failed visual
acceptance check, not evidence of a crash.

Cause: moved_picker marked the screen dirty; draw() cleared the whole display.
draw_map() then cleared the map rectangle again whenever asynchronous rendering
returned pending. The worker's one mutable canvas cannot safely be read while
rendering, but the already displayed complete frame can remain on the panel.

Correction in map_screen.cpp: movement/zoom redraws preserve the existing map
rectangle when a complete view is visible. Pending requests show Updating map...
over that old view; the new complete worker result replaces it normally. New
screen sessions and full redraws clear the preservation state. No second
framebuffer, concurrent canvas read, altered projection or radio changes.
This removes blanking; it does not implement moving cached geography. While
pending, displayed geography is the previous view and coordinates are proposals.

Focused test first reproduced full-screen/map-area clear counts, then passed
after correction. Real screen tests include drag/tap and save/cancel return.
Command from integration worktree under WSL:
`cmake --build .local/maps/labels-host --target orcsdr_maps_tests -j4`, then
`ctest --test-dir .local/maps/labels-host -R '^map_pipeline$' --output-on-failure`.
Result **1/1 passed**. Logs: picker-blackout-red-build-20261018.log,
picker-blackout-red-20261018.log, picker-blackout-green-build-20261018.log,
picker-blackout-green-20261018.log.

## Approved native/COM17 validation

User approved one build/app-only test. Built b9140473 plus blackout and preserved
wizard edits successfully: `apps/orcsdr-tab5/blackout-native-build-20261018.log`.
The native pin/miniz/FT8 gates passed. Verified COM17 identity and previous app,
saved/verified current NVS, wrote only app, and verified protected flash regions.
`blackout-flash-run-20261018.log` and individual operation logs report PASS.
Artifacts/manifest: `.local/maps/blackout-b9140473-20261018/`. Immediate rollback
is the saved gesture candidate. COM39 untouched; C6 remains 3.0.6.

Boot passed with SD/Wi-Fi/RTL ready (`blackout-boot-20261018.log`). Ran the
identity-checked bench with `gesture-test-plan-20261018.json` and output
`blackout-test-20261018.log`: six pan/zoom actions all reached ready with zero
missing/failed tiles; final RF remained STREAMING with drops=0 and overruns=0.
Device left on map picker while receiving. User then physically confirmed
"Map stays visible" in response to the drag/tap/zoom blackout check. This closes
that visual defect's acceptance gate on COM17; serial completion alone was not
used to establish it. Rendering speed is not claimed improved by this fix.

Source remains alongside preserved wizard edits. Fluid cached motion, memory
headroom and the remaining original plan gates stay open.
