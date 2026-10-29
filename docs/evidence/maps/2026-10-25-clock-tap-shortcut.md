# Clock tap shortcut

The user requests that tapping the displayed time/clock open the existing time
set page. Home's time/date segment and FT8's UTC display and clock chips now
return the named `open_clock` action. Both application handlers call one
`open_clock_settings` routine, which uses existing Settings navigation and opens
its clock editor. NTP remains an explicit action inside that editor. No time
change occurs just by opening it, and the existing Save/Cancel behavior remains.
Authenticated `RTL_UI OPEN CLOCK` uses the same routine; no coordinate tapping
or new authentication route is required.

Home popup handlers retain priority. FT8's existing overlay handlers run before
the clock hit regions. Clock editor entry updates the existing bounded Settings
body rather than clearing the whole screen again. No framebuffer or memory-pool
change is added. Independent serial FT8 NTP control remains available.

Source checks, run from `F:\AI\OrcSDR-TEMP\maps-integration-completion`:

- Focused FT8 dashboard test initially failed to compile for the missing action
  (red), then PASS after implementing the UTC and chip actions. Logs
  `clock-tap-red-20261025.log`, `clock-tap-ft8-green-20261025.log`.
- Focused CMake `orcsdr_maps_tests` build and CTest `-R '^map_pipeline$'`: PASS
  1/1, including direct editor entry, visible controls, no extra full-screen
  clear, and inactive editor rejection. Logs
  `clock-tap-settings-{build,test}-20261025.log`.
- `python .local/maps/check_clock_tap_syntax_20261025.py home_dashboard.cpp ft8_dashboard.cpp settings_app.cpp main.cpp`:
  all four real P4 syntax checks exit 0; `clock-tap-p4-syntax-20261025.log`.
- Home's existing device self-check includes the time/date hit region; native
  execution is pending. Native link and physical touch acceptance are not yet
  claimed at this checkpoint.

Raw logs stay beside invocation outside Git. COM39's map download is not touched.

The combined `9e3f0378` candidate is now installed on COM17. Native build,
flash/preservation, startup Home self-check and named `RTL_UI OPEN CLOCK` passed.
Its captured editor was inspected; Save, Cancel and Sync NTP are visible.
See [combined device evidence](2026-10-25-map-quick-file-check.md). COM17 was
returned to Home for the pending physical time/date tap check; no clock value
was saved or changed by these tests.

Physical acceptance: the user subsequently tapped Home's time/date on COM17 and
confirmed **"Opens Set Clock correctly"** on installed source `9e3f0378`.
This closes Home's physical touch check. FT8 clock-chip/UTC routing has host
coverage; no separate physical FT8 tap result was reported.
