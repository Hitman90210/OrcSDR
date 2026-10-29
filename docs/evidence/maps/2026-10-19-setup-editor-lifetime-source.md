# Setup editor teardown — source checks, October 19, 2026

Bounded review found external navigation out of setup released reception but
left its password/address editor active. Outside maps, route_key prioritizes an
active shared editor, so subsequent input could repaint the abandoned field.
A delayed cleanup must also avoid closing an editor opened by another screen.

The regression first failed: setup_ui::reset replaced a newer editor. Initial
assert-enabled Windows run produced a CRT dialog and was terminated by verified
exact executable path. The repeat reports failure via return1 without a dialog.
Logs setup-editor-lifetime-red-{build,test}*20261019.log retain both outcomes.

Minimal correction: text_editor begin increments a uint64 generation, exposed
read-only. setup_ui records its generation, clears text/closes only while still
owner, always drops its local input flag. Invalid-coordinate reentry refreshes
the token. map_screen resets setup UI on normal close and the once-only external
exit branch before releasing the setup reception pause. New unrelated editor
is preserved. No NVS/location/reception start policy change.

Focused commands from integration root:

    cmake --build .local/maps/timing-host-20261019 --config Debug --target orcsdr_setup_ui_tests orcsdr_maps_tests
    ctest --test-dir .local/maps/timing-host-20261019 -C Debug -R '^(setup_ui|map_pipeline)$' --output-on-failure

PASS2/2; final real-editor build/test logs:
setup-editor-lifetime-real-build-20261019.log,
setup-editor-lifetime-real-test-20261019.log. Pipeline target now links real
text_editor instead of no-op stubs. Tests cover abandoning setup password,
cleared inactive editor, pause release, repeated-tick idempotence and preserving
a newer editor, plus existing save/failure/cancel checks. Separate setup UI test
covers generation mismatch cleanup with real editor. No full suite run.

    python .local/maps/check_setup_editor_lifetime_syntax_20261019.py

P4 syntax4/4 pass map_screen/setup_ui/settings_app/text_editor; log
setup-editor-lifetime-p4-syntax-20261019.log. No native link or device deployment
yet. Existing preserved wizard edits remain; this source report does not close
whole setup/download or first-boot acceptance.
