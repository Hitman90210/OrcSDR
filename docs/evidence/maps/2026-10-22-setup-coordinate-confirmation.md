# Setup coordinate review and serial save parity — October 22, 2026

The self-contained coordinate input saved immediately after longitude entry,
although the screen instructed users to confirm before saving. Its review view
did not show the entered coordinates. The correction retains a proposal,
displays latitude/longitude at text size 4, and writes only after SAVE LOCATION.
Back can abandon the proposal without replacing the saved receiver location.
Failed saves keep the proposal and retry button; existing Saving feedback stays.

Related serial audit: `RTL_ADSB_LOCATION <latitude> <longitude>` lacked the
authentication check present on other location mutation commands and duplicated
persistence/update logic. It now requires authentication and calls the same
verified `save_map_receiver_location` hook used by the picker and wizard.
Arguments/ranges, result names and the ADS-B refresh behavior are retained.
This is an explicit save command, not a coordinate-proposal command.

Changed source: `apps/orcsdr-tab5/ui/setup_ui.cpp`, `ui/main.cpp` and
`tests/maps/setup_ui_tests.cpp`. No network, receiver, DSP or SD implementation
is changed. No production catalog/key or hosting configuration is introduced.

## Focused evidence

The new no-save-before-confirm assertion fails against the original code at
`setup_ui_tests.cpp:58`; saved log:
`setup-coordinate-confirm-red-test-20261022.log`. Source correction then passes:

```text
wsl bash -lc 'cd /mnt/f/AI/OrcSDR-TEMP/maps-integration-completion && cmake --build .local/maps/labels-host --target orcsdr_setup_ui_tests orcsdr_setup_tests -j2 > setup-coordinate-confirm-host-build-corrected-20261022.log 2>&1 && ctest --test-dir .local/maps/labels-host -R "^setup_(ui|flow)$" --output-on-failure > setup-coordinate-confirm-host-test-20261022.log 2>&1'
wsl bash -lc 'cd /mnt/f/AI/OrcSDR-TEMP/maps-integration-completion && cmake --build .local/maps/map-serial-asan-20261021 --target orcsdr_setup_ui_tests orcsdr_setup_tests -j2 > setup-coordinate-confirm-asan-build-20261022.log 2>&1 && ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir .local/maps/map-serial-asan-20261021 -R "^setup_(ui|flow)$" --output-on-failure > setup-coordinate-confirm-asan-test-20261022.log 2>&1'
wsl bash -lc 'cd /mnt/f/AI/OrcSDR-TEMP/maps-integration-completion && cmake --build .local/maps/labels-host --target orcsdr_maps_tests -j2 > setup-coordinate-confirm-map-build-20261022.log 2>&1 && ctest --test-dir .local/maps/labels-host -R "^map_pipeline$" --output-on-failure > setup-coordinate-confirm-map-test-20261022.log 2>&1'
python .local/maps/check_setup_confirm_syntax_20261022.py
```

Results: setup UI/flow **2/2 PASS** normally and with ASan/UBSan; map pipeline
**1/1 PASS**; P4 syntax **2/2 PASS** (`setup_ui.cpp`, `main.cpp`). The UI test
covers no save on longitude acceptance, visible review values, failed save/retry
and cancellation preserving the old point. Initial host build used a mistaken
target name `orcsdr_setup_flow_tests`; that invocation failed before CTest and
was corrected to the actual `orcsdr_setup_tests` target. Its failed log is kept.

All logs are in the worktree root. P4 syntax log:
`setup-coordinate-confirm-p4-syntax-with-serial-20261022.log`.
These host tests stub storage and radio; the serial command authentication
branch is compiled for P4 but not executed by them. Native link and COM17
authentication/UI/save/cancel checks remain next. Full first-boot, all location
methods and interrupted-setup acceptance are not closed by this correction.

## Native build and COM17 results

Installed source: `7b2b1228`. Device: COM17, USB serial `30:ED:A0:E2:E6:95`;
COM39 untouched. Commands from the integration worktree root:

```text
python .local/maps/build_fit_order_20261020.py build setup-confirm-20261022
C:/Espressif/python_env/idf5.5_py3.14_env/Scripts/python.exe .local/maps/inspect_setup_confirm_20261022.py
python .local/maps/com17_label_bench.py setup-confirm-preflash-plan-20261022.json setup-confirm-preflash-device-20261022.log
python .local/maps/flash_setup_confirm_20261022.py
```

All exit 0. Current app identity, recovery and fresh non-app backups are verified
before app-only writing; post-write preservation verifies NVS/world/C6 and
bootloader/table. No SD files are replaced. Artifact directory:
`.local/maps/setup-confirm-7b2b1228-20261022/`. App: 5,139,536 bytes, factory
headroom: 1,151,920 bytes. SHA-256:
`f4f61c666b9010c9d059aae31bd5fb402be6fb7b729839c4cfe59d240e363734`.
ELF SHA-256:
`b3d7cc37eecfe49d55cc4b5b783dd1febf9c43f9436af3e627c06872590aaf17`.
The FT8 comparison snapshot remains in PSRAM, the internal DMA reserve remains
40 KiB, and existing symbol/dependency/C6/configuration gates pass. Rollback is
the verified `89109160` app.

Saved root logs: `fit-order-native-setup-confirm-20261022-20261020.log`,
`setup-confirm-artifact-inspection-20261022.log`, preflash device/console logs,
and `setup-confirm-{verify-previous,backup-*,verify-backups,app-write,preservation}-20261022.log`.
Native monitor capture from its own invocation directory:
`apps/orcsdr-tab5/setup-confirm-boot-native-monitor-20261022.log`; SD, Wi-Fi,
RTL and normal app startup reach Ready. The monitor was closed before testing.

### Actual checks and test-harness failures

1. After reboot, before authentication, a valid same-point
   `RTL_ADSB_LOCATION` returns `RTL_ADSB_LOCATION_ERROR auth_required`.
   The generic harness erroneously treats this expected denial as an expired
   session and exits 1 (`setup-confirm-device-20261022.log`). A local copy of
   the harness permits a denial only for an explicitly unauthenticated step
   whose expected result ends with `auth_required`.
2. That corrected plan again proves denial, then opens setup and confirms the
   driver is IDLE. Entering 10.25/20.5 leaves the old point unchanged; Back
   discards that proposal. Entering it again also leaves the old point unchanged.
   The Save tap is issued, but the two-second collection window expires before
   its acknowledgment, so the plan exits 1
   (`setup-confirm-device-corrected-20261022.log`).
3. Later diagnostic readback proves that explicit Save did persist the intended
   temporary point. Setup is cancelled and reception resumes. The serial shared
   save restores the original point and acknowledges after about 4.2 seconds.
   That restoration plan exits 1 on an incorrect expected return screen: setup
   had been opened from post-boot Home, so Cancel correctly returned to Home,
   not ADS-B. Logs: `setup-confirm-input-diagnostic-device-20261022.log` and
   `setup-confirm-restore-device-20261022.log`. Neither failure is hidden or
   labelled a complete-plan pass.
4. The final bounded plan explicitly enters ADS-B first and uses ten-second
   save windows. It exits 0: manual entry leaves the saved point unchanged,
   Save persists 10.25/20.5, serial save restores the exact original E7 point,
   Cancel returns to ADS-B and reception is STREAMING. The UI save acknowledgment
   takes **3.258 seconds** from the tap. This timing includes command processing
   and persistence; it is not a render-time measurement. Saving feedback is
   covered on host, with physical appearance still unreviewed for this change.

Successful command:

```text
python .local/maps/com17_label_bench.py setup-confirm-device-save-window-plan-20261022.json setup-confirm-device-save-window-20261022.log
```

Logs remain in the root, with corresponding `setup-confirm-console-*.log` files.
`setup-confirm-evidence-check-20261022.log` independently checks no save marker
before the explicit tap, changed-point readback, original-point restoration,
reception pause/resume and the denial result. Current receiver coordinates are
`440628866,-1230160273`, restored from this run's preflash record. Earlier
historical location values were not used as defaults or restored over them.

Final observation: ADS-B at 1090 MHz, AUTO gain 0, RTL AGC off; about 2,046,458
samples/s, zero observed driver drops/overruns. Free PSRAM 20,657,780 bytes,
largest PSRAM block 20,447,232; largest DMA block 8,704; main-stack high-water
margin 4,172; uptime 403,821 ms with no unexpected reset in these checks.
The pretest AUTO gain field was 280 tenths dB; its exact value is not restored.
All monitor/test sessions are closed. These bounded coordinate-flow checks do
not close network/address methods, physical hotplug, full first boot, interrupted
setup, actual HTTPS map installation, RF decoding or performance acceptance.
