# Background download notice — source validation

October 24 follow-up: [native COM17 build, preservation and navigation pass](2026-10-24-download-feedback-device.md), with physical header readability accepted. The source-only status below is the earlier checkpoint; real full-file numeric progress remains open.

The clean GitHub installation and offline physical appearance passed on COM17.
The installed firmware remains `137e244a`. The following changes are prepared
source, not yet installed or physically accepted.

The shared dashboard header now has a bounded map status button: download or
verification percentage, Ready, Paused or Error. Tapping it opens Get Maps
directly. Settings and standalone map/documentation/visualizer pages retain
their existing layouts. Keypads, text editors, Home popups and the expanded
audio control block the notice's drawing and input. This is scoped coverage,
not an application-wide modal audit.

The UI owner polls the existing worker snapshot every 250 ms. Unchanged values
do not repaint; changing values repaint only the 94×54 button. Brand repaint
preserves its reserved header slot. The new static snapshot adds 152 bytes of
PSRAM by declaration; native linking/placement remains unverified. No image
buffer, task or new storage/network operation is added.

Serial parity uses authenticated `RTL_MAP OPEN DOWNLOADS`; read-only
`RTL_MAP STATUS` reports `downloads=1` for that view. This enters the page once
through the existing screen lifecycle, without an intermediate inventory draw.

## Checks actually run

```sh
cmake --build .local/maps/feature-replay-integration-20261023 --target orcsdr_maps_tests orcsdr_map_download_notice_tests -j2
ctest --test-dir .local/maps/feature-replay-integration-20261023 -R '^(map_pipeline|map_download_notice)$' --output-on-failure
```

WSL: **2/2 passed**. The recording display checks changing progress without
full-screen clears, unchanged updates, invalidation, screen ownership, blocked
input, terminal states and focus-target deduplication. The map pipeline checks
authentication, observable state and a single screen-entry clear.

Five P4 syntax checks passed: `map_download_notice.cpp`,
`dashboard_audio_control.cpp`, `map_screen.cpp`, `map_serial.cpp`, `main.cpp`.
Exact compile invocations are in
`.local/maps/check_background_notice_syntax_20261024.py`.

Logs saved in the invoking worktree root:
`maps-background-notice-final-host-20261024.log` and
`maps-background-notice-p4-syntax-20261024.log`.
The initial target-discovery build failure is retained in
`maps-background-notice-host-20261024.log`; the corrected build passed.

## Remaining gates

Native build/link, device touch/appearance and worker-to-display publication
remain unverified. No new archive transfer or flash was needed for these host
checks. Combine native validation with numeric verification feedback, without
repeating the 543 MB baseline solely for a UI change. COM39 is untouched.

This notice does not change reception policy. Downloads and fresh map rendering
still share a worker; it does not establish concurrent smooth map refills or
active reception during HTTPS transfer. Retry counter continuity remains open.
