# Setup Maps progress redraw correction

The user reported excessive flicker on setup page 4/5 during the real global-pack
download, and explicitly requested flicker prevention as a continuing project
coding practice. The current transfer is left running; no flash/restart is used
to investigate this drawing defect.

## Root cause and correction

`map_screen::tick()` used each download snapshot revision to mark the entire
page dirty, even on the Installed view. The worker advances progress revisions
frequently. `draw()` then calls `fillScreen()` and rebuilds the layout. Verification
also forced that redraw every 250 ms without any state change.

The correction distinguishes layout/control/status-phase changes from byte-only
updates. The latter repaint only the existing 1216×44 status/progress region on
the Downloads view; the Installed view ignores invisible byte changes.
Verification animates that same region. Screen ownership and worker/UI boundaries
are unchanged. No sprite/framebuffer allocation or download behavior change.

Permanent practice is recorded in `apps/orcsdr-tab5/ui/AGENTS.md` and
`docs/UI_RENDERING_STANDARD.md`: value/progress/animation updates must preserve
static layout; use focused drawing regressions; verify physical behavior separately.
At the user's explicit request, a small memory update note is saved outside the
repository under the approved memory extensions directory. No memory registry is
edited. This is not a claim that all historical screens have been audited.

## Tests executed

From `F:/AI/OrcSDR-TEMP/maps-integration-completion`:

```text
wsl -d Ubuntu -- bash -lc 'cd /mnt/f/AI/OrcSDR-TEMP/maps-integration-completion && cmake --build .local/maps/labels-host --target orcsdr_maps_tests -j2 && ctest --test-dir .local/maps/labels-host -R "^map_pipeline$" --output-on-failure'
wsl -d Ubuntu -- bash -lc 'cd /mnt/f/AI/OrcSDR-TEMP/maps-integration-completion && cmake --build .local/maps/map-serial-asan-20261021 --target orcsdr_maps_tests -j2 && ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir .local/maps/map-serial-asan-20261021 -R "^map_pipeline$" --output-on-failure'
python .local/maps/check_picker_navigation_syntax.py
```

The regression first fails the full-screen-clear assertion on the original
source (`maps-transfer-flicker-red-corrected-20261022.log`). An initial mistyped
CTest filter matches no tests and supplies no passing evidence; retained log
`maps-transfer-flicker-red-20261022.log`.

After correction: normal 1/1 PASS, ASan/UBSan 1/1 PASS, P4 syntax for map screen,
service, pipeline, Settings and main PASS. Logs:
`maps-transfer-flicker-{host,asan,p4-syntax}-20261022.log`.
Checks cover repeated download bytes with updated visible text, verification
animation without new revisions, and invisible progress on Installed. The fake
clock is now controllable; its default retains the prior 1000 ms value.

Native build and artifact inspection now pass, and COM17 is app-only updated to
`ef53789f` after the global download completes. Partition preservation and matching
boot pass; installed global pack and persisted trust remain intact. LoRa's new
regional basemap renders and a native capture is saved. Physical progress-page
flicker acceptance remains open; a still capture cannot prove its absence.
[Exact artifacts and device evidence](2026-10-22-download-pilot-publication.md).
COM39 untouched.
