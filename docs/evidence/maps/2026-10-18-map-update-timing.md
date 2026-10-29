# Bounded map update intervals — source evidence

Phase 3 continuation. Added `map_timing.hpp`, focused `map_timing_tests.cpp`,
host CMake target, and map_service integration. Fixed rolling array of 256
uint64 intervals, external-BSS service object; no per-update allocation.
Records only distinct frame revisions after UI presentation completes.
Reports nearest-rank p50/p95/max of retained samples through RTL_MAP_STATUS.
Repeated pushes of the same revision are excluded. Clock discontinuities
restart the baseline. Samples include user dwell, explicitly marked
`raw_includes_dwell=1`; they are not FPS or touch-to-feedback measurements.
Use a controlled continuous gesture run when interpreting these intervals.

Test-first compile failed because the header was absent:
`map-timing-red-20261018.log`. Then compiled with
`c++ -std=c++17 -Wall -Wextra -Werror -I apps/orcsdr-tab5/ui apps/orcsdr-tab5/tests/maps/map_timing_tests.cpp -o .local/maps/map-timing-test`
and ran `.local/maps/map-timing-test`: **PASS**.
Log: `map-timing-green-20261018.log`, from the integration worktree under WSL.
Checks empty state, repeated frame suppression, percentile calculation,
baseline reset and rollover beyond capacity.

Host CMake registration committed separately as `bf888074`, preserving the
uncommitted wizard test configuration. Configured the existing labels-host
build, built only `orcsdr_map_timing_tests`, then ran
`ctest --test-dir .local/maps/labels-host -R '^map_timing$' --output-on-failure`:
**1/1 passed**. Logs: `map-timing-ctest-config-20261018.log`,
`map-timing-ctest-build-20261018.log`, `map-timing-ctest-20261018.log`.

`python .local/maps/check_map_timing.py`: **two P4 syntax checks pass**,
map service and pipeline. `map-timing-syntax-20261018.log`. An initial invocation
of the older check script refused to overwrite its existing log; no old log
was overwritten. No native build/link or hardware change occurred.

This instrumentation is prepared alongside the source-only gesture fix and
preserved wizard work. Actual external-BSS linking, device readings and
performance improvement remain unverified. It does not implement the missing
cache: engine APIs provide keys/projected features, not a complete rendered-tile
cache. The current worker canvas is mutable while rendering, so concurrent UI
reuse requires an explicit ownership design before cached-motion integration.
