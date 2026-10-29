# Legacy map wording cleanup

The inventory already called old ORCMAP1 data "Older map file (unsupported)",
but Details still displayed its internal `lane_county_map` identity. Details now
shows "Legacy ORCMAP1 format (unsupported)" for that entry. Actual PMTiles pack
identities and user-supplied geographic metadata are unchanged.

Changed `apps/orcsdr-tab5/ui/map_screen.cpp` and its recording-display regression
in `apps/orcsdr-tab5/tests/maps/map_screen_tests.cpp`. No buffer, touch action,
layout or repaint policy was added. Compatibility detection still recognizes
`/sd/orcsdr/data/lane_county_map.idx` without deleting or rendering it; read-only
`RTL_MAP LIST` retains the identity for diagnostics. Historical documentation
and developer catalog examples still contain the old name; this is not a claim
that every repository reference was purged.

From the integration worktree, the focused regression failed before the fix
at the new display assertion, then passed after it:

```text
wsl bash -c 'cd /mnt/f/AI/OrcSDR-TEMP/maps-integration-completion && cmake --build .local/maps/feature-replay-integration-20261023 --target orcsdr_maps_tests -j2 && ctest --test-dir .local/maps/feature-replay-integration-20261023 -R "^map_pipeline$" --output-on-failure'
python .local/maps/check_legacy_wording_syntax_20261023.py map_screen.cpp
```

Results: focused test 1/1 passed; P4 syntax exited 0. The test verifies both
generic visible wording and retained serial diagnostic identity. Existing map,
setup and redraw assertions in that program also passed.

Logs saved beside invocation:
`maps-legacy-wording-red-20261023.log`,
`maps-legacy-wording-host-20261023.log`,
`maps-legacy-wording-p4-syntax-20261023.log`.

No hardware used or firmware flashed. COM17 remains on `f38b8601`; COM39 untouched.
Physical acceptance of this wording awaits the next combined native candidate.
The performance, production delivery and final review gates remain open.
