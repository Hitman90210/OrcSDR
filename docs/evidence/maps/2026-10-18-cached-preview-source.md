# Cached movement adapter — prepared source, not hardware acceptance

Worker decoding holds g_lock for the whole frame. Prepared a separate PSRAM
snapshot and publication mutex in map_service.cpp. Worker copies only completed
images; UI reads only the snapshot, never the mutable worker canvas. UI uses
zero-wait locks; publication can wait for a UI push. Lock order is worker lock
then snapshot lock, never the reverse. Both buffers are included in reclaim
accounting and deleted together. Snapshot creation checks PSRAM headroom and
uses setPsram(true); failure retains the existing delayed-render fallback.

Same-size pan uses translated pushSprite; zoom uses existing native scaling
for scales 0.25–4. Shared OrcMaps MakeFrameTransform owns camera reprojection.
UI clips to the intersection of parent clip and map rectangle and restores the
parent clip. Exposed regions receive background. Snapshot includes geographic
labels; application overlays still use requested camera. Cancel/rescan/new
screen invalidates preview reuse. Preferred pack identity and dimensions must
match. Worker result remains generation-gated before publication.

Additional picker snapshot is 1,417,728 bytes RGB565; no DMA reserve change.
CPU-only canvas object is marked external BSS. Runtime allocation/placement
and copy/scaling cost remain unverified. No 33 ms claim: measured existing
full-frame pushes already cost approximately 77 ms.

Shared transform focused host test passes 1/1 in the OrcMaps worktree.
First integrated P4 syntax checks pass (map service and pipeline), log
`map-preview-syntax-20261018.log`. Final syntax checks after cancel invalidation
and bounded scale guards also pass: `map-preview-final-syntax-20261018.log`.
No native build or
flash. Engine transform 63f0573 remains local pending requested push approval;
OrcSDR pin still 5f3a1eb and cannot build this header until dependency resolution.

Required before acceptance: unit/integration tests for publication/cancellation,
snapshot failure/reclaim, invalidation and clipping; approved pin update/native
build; PSRAM, SD/RF and actual gesture latency
checks. This is prepared integration, not a finished cache or completed Phase 3.

## Lifetime-policy follow-up

Found and removed a cancellation race in the prepared boolean invalidation:
late worker publication could set the flag true after cancellation. Requests
now carry a session lifetime; cancellation/rescan/reclaim increments it, and
preview reads reject a different lifetime even if a late snapshot publishes.
Camera changes within the same session can still reuse the immutable image.

Added map_preview.hpp and focused map_preview_tests.cpp, testing lifetime,
pack/dimension identity, invalid/reclaimed snapshot, null output and scale
limits. First missing-header compile failed; implementation then passed with
Wall/Wextra/Werror, log `map-preview-policy-green-20261018.log`.
Registered CTest `map_preview`, 1/1 passed; config/build/test logs named
`map-preview-policy-config-20261018.log`, `map-preview-policy-build-20261018.log`,
`map-preview-policy-ctest-20261018.log`. Final two P4 syntax checks also pass:
`map-preview-lifetime-syntax-20261018.log`.

These tests exercise the actual eligibility helper used by the service, not
concurrent mutex scheduling, sprite allocation failures or display clipping.
Those broader integration checks and native runtime acceptance remain open.

## Approved engine publication and resolved dependency

User approved development-branch push of OrcMaps 63f0573. Push succeeded and
remote refs/heads/codex/map-quality-delivery was verified at full SHA
`63f0573e98351ef50eb3debeca5ee07a0a9f2eeb`. No main merge, tag or release.
OrcSDR manifest now pins that revision. Component manager regenerated the lock
and fetched the component; all unrelated entries compare unchanged. Resolution
log: `preview-pin-resolution-20261018.log`. `check-orcmaps-pin.py` confirms
requested/locked/fetched agreement. Managed source was not hand-edited.

Configured a separate focused host build against the actual managed component:
`cmake -S apps/orcsdr-tab5/tests/maps -B .local/maps/preview-pinned -DORCMAPS_ROOT=$PWD/apps/orcsdr-tab5/managed_components/orcmaps`.
Built only orcsdr_map_preview_tests, orcsdr_map_timing_tests and orcsdr_maps_tests,
then `ctest --test-dir .local/maps/preview-pinned -R '^(map_preview|map_timing|map_pipeline)$' --output-on-failure`:
**3/3 passed**. Logs: preview-pinned-config-20261018.log,
preview-pinned-build-20261018.log, preview-pinned-tests-20261018.log.
This supersedes the earlier pending-push statement; native build and hardware
preview performance remain unverified. COM17 is unchanged by resolution/tests.

## Display clipping follow-up

Moved the actual native preview painting into the small templated adapter seam
`paint_preview` used by both map_service and a recording graphics target.
Focused test first failed to compile with the seam absent, then passed 1/1.
Checks translated and scaled paths receive only the intersection of parent/map
clips, original clipping is restored, and an empty intersection performs no
clear or push. Clip-edge arithmetic uses int64 intermediates.
Logs: `map-preview-clip-red-20261018.log`, `map-preview-clip-build-20261018.log`,
`map-preview-clip-green-20261018.log`. Command: build only
`orcsdr_map_preview_tests`, then CTest `-R '^map_preview$' --output-on-failure`.
Two real-SDK P4 syntax checks pass after extraction:
`map-preview-clip-syntax-20261018.log`.

The recording target verifies calls/clip boundaries; it is not pixel-level
M5GFX raster verification. Concurrent scheduling and actual allocation/reclaim
failures still require adapter integration or hardware tests. No device changed.
