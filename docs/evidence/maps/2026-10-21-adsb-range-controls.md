# ADS-B range: shared UI/serial state

Source commit: `19856eea`. The earlier coverage inventory missed an existing
named route, `RTL_UI ACTION SETTINGS RANGE <nm>`. Inspection found that its
shared Settings action updated `adsb_settings` and scheduled persistence, but
an already active dashboard retained its separate `g_settings` range.
`draw_adsb_dashboard(false)` does not re-enter that active dashboard. This is
a source-confirmed synchronization gap; no before/after device reproduction
is claimed yet.

The correction adds a validated dashboard setter used by the physical zoom
buttons, dashboard range selector and shared Settings action. It invalidates
map presentation on change without drawing from a non-owning screen. Existing
receiver coordinates, aircraft state, view and selection are preserved. The
shared Settings action rejects unsupported radii; its serial route also rejects
missing or numerically unsupported values before narrowing.

`RTL_ADSB_RANGE` queries configured/dashboard values, active state and pending
persistence. `RTL_ADSB_RANGE 10|25|50|100` is an authenticated, strictly parsed
setter through the same Settings action. Its acknowledgement explicitly says
that persistence is pending. No new storage path or basemap engine was added.

Changed files: `apps/orcsdr-tab5/ui/adsb_dashboard.cpp/.hpp`,
`apps/orcsdr-tab5/ui/main.cpp`,
`apps/orcsdr-tab5/tests/maps/adsb_map_range_tests.cpp` and its CMake target.

## Tests actually executed

From the integration worktree, WSL CWD
`/mnt/f/AI/OrcSDR-TEMP/maps-integration-completion`:

```text
cmake -S apps/orcsdr-tab5/tests/maps -B .local/maps/labels-host
cmake --build .local/maps/labels-host --target orcsdr_adsb_map_range_tests -j2
.local/maps/labels-host/orcsdr_adsb_map_range_tests
```

PASS: exact supported protocol values; malformed, overflowing, null and extra
input leaves the prior output unchanged. Initial attempts failed because the
new target needed CMake regeneration, then a missing `<initializer_list>`
include; both are corrected. Logs are retained:

- `adsb-map-range-host-configure-20261021.log`
- `adsb-map-range-host-build-20261021.log`
- `adsb-map-range-host-build-corrected-20261021.log`
- `adsb-map-range-host-build-final-20261021.log`
- `adsb-map-range-host-test-20261021.log`

Native commands, from the Windows integration worktree:

```text
python .local/maps/check_adsb_map_range_syntax_20261021.py
python .local/maps/build_fit_order_20261020.py build adsb-map-range-20261021
C:/Espressif/python_env/idf5.5_py3.14_env/Scripts/python.exe .local/maps/inspect_adsb_map_range_20261021.py
```

Both changed P4 translation units pass syntax checks; the native build exits 0.
Inspection passes dependency/symbol/PSRAM gates and retains the 40 KiB internal
DMA reserve. App: 5,133,904 bytes; factory headroom: 1,157,552 bytes.
SHA-256: `6e19d31768ff8425ac4c343454381c9d30ef0495c209b29f63ad36b2bce65e11`.
Candidate: `.local/maps/adsb-map-range-19856eea-20261021/`.

Logs: `adsb-map-range-p4-syntax-20261021.log`,
`fit-order-native-adsb-map-range-20261021-20261020.log`,
`adsb-map-range-inspection-20261021.log`.

## Hardware gate still open

COM17 USB identity remains `30:ED:A0:E2:E6:95`. Preflash serial opening failed
with Windows access denied; another program owns the port. Logs:
`adsb-map-range-preflash-console-20261021.log` and
`adsb-map-range-preflash-20261021.log`. No reset, flash, or SD/NVS mutation
was performed by that attempt. Do not interrupt the other serial owner.
COM17 therefore remains on `ba0e7a92`; COM39 was not accessed.

Next device checks: named-set versus live dashboard state and rendered camera,
physical +/− route, malformed/auth rejection, restored original range and
reboot persistence. Full decoder, global-pack delivery and 33 ms interaction
acceptance remain open. Next implementation milestone is LoRa's shared camera
interaction with named pan/zoom/receiver/selected-node/follow controls.
