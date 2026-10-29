# Connected receiver and audio, but blank Home plots

User reported COM17 connected, tuning/navigation and sound working, with no
spectrum or waterfall. Read-only serial diagnostics (no reset/flash) found
`RTL_UI_STATUS screen=home ... graphics=0 radio_ui=1`; capture running, driver
streaming approximately 2.38 MSPS, zero driver drops/overruns, IQ samples and
audio chunks advancing. This was disabled graphics, not evidence of USB loss.

Logs in integration root: `home-empty-diagnostic-20261018.log` and plan/run
files with the same prefix. Status commands included `RTL_UI STATUS`,
`RTL_SCREEN_STATUS`, `RTL_CAPTURE_STATUS`, `RTL_DRIVER STATUS`,
`RTL_DSP STATS`, `RTL_WIFI_COEX_STATUS`, `RTL_HEALTH`.

Recovery used existing authenticated `RTL_UI ACTION AM GRAPHICS` to toggle
runtime plotting, followed by status/DSP checks. `graphics=1`, streaming and
zero drops/overruns verified. User subsequently confirmed **"it came back now"**.
No firmware reset, flash, USB power cycle or C6 update. User was navigating
concurrently; recovery status observed CB rather than Home, so do not attribute
that screen/frequency change to a graphics toggle or silently retune the device.
Recovery log: `home-graphics-restore-20261018.log` and associated plan/run.

Source finding: `ui/navigation_service.cpp::open_settings` unconditionally
assigns `g_restore_graphics = disable_graphics()` on every invocation. A second
open while Settings already owns the temporary graphics pause therefore saves
false instead of the original true. Close/Home later restores false. The setup
test plans used repeated Settings opens, so this is a concrete likely trigger;
the live diagnostic alone cannot prove which invocation disabled this session.

Next narrow correction: retain the original graphics restore value across
repeated opens of the same Settings session; still allow section changes. Test
repeated open → Close/Home with both initially enabled and disabled graphics.
## Approved source correction and host regression

User approved source/host work only. `ui/navigation_service.cpp` now records
the graphics restore value only when entering a new Settings session; repeated
opens still select/redraw the requested section. One-line product change,
uncommitted. Preserve the prior false value when graphics were intentionally off.

`tests/navigation_graphics_tests.py` exercises the actual open/close/Home
functions and real `screen_controller.cpp` with fake display/dashboard hooks.
Initial regression failed at `graphics==initially_on` before the fix. The first
test draft's simplified screen fake also produced a return-target failure;
inspection of the real controller showed it already protects that target.
The fixture was corrected to link the real controller; no unrelated screen
controller product change was made.

From integration root under WSL:

```sh
python3 apps/orcsdr-tab5/tests/navigation_graphics_tests.py
```

Pass: repeated opens retain graphics and requested section, Close and Home
restore the original state, and intentionally disabled graphics stays disabled.
Logs: `navigation-graphics-red-20261018.log`,
`navigation-graphics-session-red-20261018.log` (discarded fake-screen mismatch),
`navigation-graphics-green-20261018.log`. Focused P4 syntax also passes via
`python .local/maps/check_navigation_graphics_syntax.py`, log
`navigation-graphics-p4-syntax-20261018.log`. Existing IDF header warning remains.

## Approved native/device regression and physical acceptance

User separately approved one native build and COM17 app-only test. Explicit
SDKCONFIG, C6 3.0.6 input and diagnostics OFF retained. Native build and
inspection pass: map pin 63f0573, bundled miniz, FT8 scratch in PSRAM, labels
15,416 bytes in external BSS and DMA reserve 40,960. App 5,060,384 bytes (20%
headroom), SHA-256 `c2450314fe356a18c7053eda1fe0c8a2280c0318a37d56c30a6aba69159fb18b`.
Build ID `maps gfx-fix dirty 2026-10-18`; HEAD 477f087a plus preserved dirty
wizard/map/setup-pause changes and the one-line graphics fix. Source remains
uncommitted, manifest includes tracked diff hash; no main merge or release.

Logs in `apps/orcsdr-tab5`: `navigation-graphics-native-build-20261018.log`,
`navigation-graphics-native-inspection-20261018.log`. Preflight and reception
stop passed. From integration root:

```sh
python .local/maps/flash_navigation_graphics_20261018.py
python .local/maps/capture_com17_boot.py navigation-graphics-boot-20261018.log
python .local/maps/com17_label_bench.py navigation-graphics-device-plan-20261018.json navigation-graphics-device-20261018.log
```

Flash verified correct COM17 identity, prior setup-pause app and fresh NVS
backup; app-only write and saved non-app partition verification passed.
Artifact/recovery manifest: `.local/maps/navigation-graphics-477f087a-20261018/`.
Immediate rollback is the saved setup-pause candidate. No C6, COM39 or SD pack
changes. Logs in root: `navigation-graphics-preflash-20261018.log`,
`navigation-graphics-flash-run-20261018.log`, operation logs with that prefix,
`navigation-graphics-preservation-20261018.log`, `navigation-graphics-boot-20261018.log`,
`navigation-graphics-device-20261018.log` and associated run/plan files.

All planned checks passed: repeated Settings opens then Close or Home retain
initial graphics state; wizard Cancel and Quick Start/Finish followed by repeated
Settings and Home do likewise. Initially OFF stays OFF; initially ON returns ON.
`navigation-graphics-device-verification-20261018.log` verifies all 14 graphics
snapshots and retained CB tuning at 27.005 MHz. Final Home graphics=1; driver
STREAMING ~2.398 MSPS, zero drops/overruns. C6 3.0.6 matches. DMA free 10,175,
minimum 9,487, largest 9,216; internal free 26,715/minimum 24,375; PSRAM free
22,108,944. Bounded evidence only; memory budget and map timing gates stay open.

User physically confirmed **"Both drawing normally"** on the corrected app.
This closes the reproduced repeated-Settings graphics-restoration regression,
not every possible cause of blank plots or any physical USB dropout issue.
COM17 left on Home, graphics enabled and receiving; serial port released.
