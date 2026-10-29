# Location-save feedback — October 19, 2026

Source-only change; not native-built, flashed or physically verified. The sealed
COM17 candidate `4eaf9fb5` plus wizard edits is unaffected. No device operations or managed-component changes were performed for this fix.
The new source hunks remain with preserved wizard edits; no source commit/push
has been made for them. Documentation is recorded separately.

## Findings and change

The picker and owned setup location pages called their synchronous save hooks
without first drawing acknowledgement. Each now draws `SAVING...` on the action
before invoking the existing hook. Success follows the previous navigation or
saved-coordinate display; failure redraws the original save action and error so
the same proposed point can be retried. This adds no delay or new save operation.

`settings_app.cpp::draw_location` showed `PROFILE LABEL: NOT SET` when the optional
location name was empty, even though a coordinate was configured and saved.
The row is now `LOCATION`, showing the existing name when configured and named,
`SAVED LOCATION` when configured without a name, or `NOT SET` when unconfigured.
A map-pack identifier is never used as a location name.

Stored optional profile labels, location records, map selection and NVS write
semantics are unchanged. The actual save remains the existing verified hook;
this patch changes acknowledgement and presentation only.

Changed source hunks:

- `ui/map_screen.cpp`: draw acknowledgement immediately before `g_hooks.location`.
- `ui/setup_ui.cpp`: draw acknowledgement before `g_hooks.save` for lookup and
  coordinate save paths.
- `ui/settings_app.cpp`: replace misleading optional-name fallback.
- `tests/maps/map_screen_tests.cpp`: callback ordering, failed-write retry action,
  saved-coordinate return and unnamed/named/unconfigured display assertions.
- `tests/maps/setup_ui_tests.cpp`: callback ordering, lookup and manually entered
  coordinate save failure/retry, and saved-state display without stale feedback.

These are the new hunks only; pre-existing dirty wizard changes are preserved.

## Focused checks

Run from `F:/AI/OrcSDR-TEMP/maps-integration-completion`:

```powershell
cmake --build .local/maps/timing-host-20261019 --config Debug --target orcsdr_maps_tests orcsdr_setup_ui_tests
ctest --test-dir .local/maps/timing-host-20261019 -C Debug -R '^(map_pipeline|setup_ui)$' --output-on-failure
```

The focused targets build and both selected tests pass. `map_pipeline` includes
the real map-screen/Settings regression functions; `setup_ui` exercises the
owned setup UI. The optional large global-archive test environment variable was
unset for these focused runs. No full host suite ran.

Logs are saved in the invoking worktree root:

- `map-location-feedback-build-20261019-01.log`
- `map-location-feedback-tests-20261019-01.log`
- `map-location-feedback-build-20261019-02.log`
- `map-location-feedback-tests-20261019-02.log`
- `map-location-feedback-build-20261019-03.log` (final strengthened retry assertion)
- `map-location-feedback-tests-20261019-03.log` (final, 2/2 passed)

The callback-order assertion proves the drawing command occurs before storage;
it does not prove physical display timing. A future authorized native candidate
still needs a physical save/failure/retry check, especially for long save hooks.


Parent verification also passed all three P4 syntax checks using the saved
ESP-IDF 5.5.4 compile database (no link):

    C:/Espressif/python_env/idf5.5_py3.14_env/Scripts/python.exe .local/maps/check_location_save_feedback_syntax_20261019.py

Log: `location-save-feedback-p4-syntax-20261019.log`. Existing SDK literal-suffix
warning only. No additional native build/flash was performed for this fix.


Reviewable source checkpoint:
`.local/maps/location-save-feedback-source-20261019/source-hashes.json` records
the five changed source/test files, including the preserved wizard content.
This source-only packet is separate from the sealed native strip candidate.
