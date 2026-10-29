# OrcSDR screen rendering standard

The user requires flicker prevention to be a normal coding practice, not a defect
they must rediscover on each new page. Apply this standard to new and modified UI.
It does not claim that every historical screen already satisfies it.

## Render layout once, update changed values

Screen entry establishes the layout, static borders, heading, buttons and hit
regions. Ordinary receiver data, download bytes, elapsed time, cursor movement
and animation update only their affected widgets. Do not call `fillScreen()` or
redraw an entire page because a generic state/revision counter changed.

Separate structural changes (entering a different page, list contents, controls
becoming available) from value-only changes (byte counts, signal level, a progress
bar). A layout transition may redraw its layout. Progress-only and verification
animation updates must keep the existing layout visible. Ignore background
progress when it has no visible effect on the active page.

Each partial region must cover the old content and the new content without
erasing adjacent controls, footer, attribution or overlays. Use opaque text or
clear only the widget's bounded rectangle. Clip moving graphics. If a small
widget still flickers, use an existing appropriately sized sprite/cache after
measuring memory and transfer costs; do not allocate full-frame buffers per tick.

## Ownership and slow operations

The owning UI task draws. Workers publish bounded state/snapshots and completion
revisions; they never draw over the currently active screen. Preserve existing
screen-controller ownership, framebuffer protections and modal/input handling.

Keep the previous completed map/image visible until its replacement is ready.
Paint immediate press/saving/working feedback before NVS, SD or network work.
Progress updates must not steal focus, reset scrolling or rebuild touch targets.
Serial actions share the same handlers, validation and observable result as UI.

## Required validation for new or changed redraw behavior

1. Use the recording display or a focused equivalent. Change data/progress several
   times and assert both the updated visible value and unchanged full-screen-clear
   count. Include unchanged animation ticks and background progress on another
   view if that state is shared.
2. Verify phase/layout transitions still update controls and clear obsolete text.
   Verify navigation, clipping, overlay ownership and immediate button feedback.
3. Run only the affected host tests and native syntax/build checks warranted by
   the change. Save exact commands and actual results. A successful host test does
   not prove that the physical display is flicker-free.
4. On authorized hardware, exercise the actual screen under representative load.
   Record physical acceptance, frame/update timing where relevant, memory/stack,
   and radio continuity separately. Never claim smoothness from desktop timing.

## Known example: setup Maps page

The October 22 download pilot exposed a full-page clear at every download revision
and every verification animation tick. The correction updates the 1216×44 status
region and leaves the layout intact; the Installed view ignores byte-only changes.
The regression changes bytes repeatedly and verifies text updates without another
full-screen clear. This is the expected pattern for future pages.

Related practices: [SD reliability](SD_CARD_RELIABILITY.md) and
[map serial commands](MAP_SERIAL_COMMANDS.md). Existing unrelated UI gaps should
be recorded and addressed in scoped changes, not a broad rendering rewrite.

## IQ continuity and the accepted map-interaction limitation

Compare driver overrun/drop counters and application IQ-pipeline drop counters
before and after each load phase. Idle reception and normal stationary use must
not accumulate UI-related losses. A responsive screen or zero driver drops alone
is insufficient evidence.

On October 26, 2026, the owner explicitly accepted small, transient IQ drops while
moving the map and filling in newly exposed detail for v0.5.0-beta.1. This narrow
exception removes that observed interaction from the release blockers; it does
not establish zero-loss rendering or a numerical loss budget. Preserve measured
counts and disclose the limitation. Continued loss after the map settles, idle
loss, crashes and unrelated reception regressions remain blockers. Never suppress
counters or silently reduce receiver rate to hide losses.

Avoid long uninterruptible graphics operations, but accept performance changes
only with measured device benefit. The October 26 experiment replacing M5GFX's
writeback hook with 4 KiB calls did not resolve IQ losses and is excluded from
the shipping build. Its failed measurements remain in the release ledger.
