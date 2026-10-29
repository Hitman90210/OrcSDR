# Named map/setup serial controls

The user requires serial equivalents for OrcSDR user actions. This checkpoint
adds named map controls while preserving the existing UI ownership, location
storage, setup pause/restore and map installer paths. It does not claim full
application-wide serial parity.

Files: `ui/map_serial.cpp/.hpp` (bounded dispatcher), `ui/map_screen.cpp/.hpp`
(shared actions and state), `ui/map_service.cpp/.hpp` (presentation invalidation),
three-line registration/help changes in `ui/main.cpp`, native/host CMake lists,
`tests/maps/map_screen_tests.cpp` and command documentation.

## Behavior

- Read-only Help/Status/local inventory/download list remain available without
  authentication. Mutations require the existing authenticated session.
- Commands are bounded to 383 characters and fixed-size tokens; extra tokens,
  invalid E7 coordinates, invalid pan bounds and bad indices are rejected.
- Opening setup returns failure if reception cannot pause. Finish is allowed
  only at Complete and requires successful persistence. Cancel restores the
  existing return screen/reception behavior. Failed automatic progress writes
  no longer advance the screen without a saved checkpoint.
- Proposed picker coordinates are distinct from saved receiver coordinates.
  Save uses the same visible Saving acknowledgement and verified location hook
  as touch. Pan/zoom and pack selection use shared map-screen actions.
- Successful label writes invalidate shared presentation and refresh an active
  picker; preferences remain independent across all four consumers.
- Download commands use the same eligibility checks and installer APIs as UI.
  They cannot bypass trust, verification, activation or recovery.
- Inventory scratch is bounded and placed in PSRAM. There is no new private
  camera, receiver, download state machine or pairing key.

[User command reference](../../MAP_SERIAL_COMMANDS.md).

## Executed host checks

Invocation root: `F:/AI/OrcSDR-TEMP/maps-integration-completion`; Linux commands
run through WSL.

```text
cmake --build .local/maps/labels-host --target orcsdr_maps_tests -j2
.local/maps/labels-host/orcsdr_maps_tests /mnt/f/AI/OrcSDR-TEMP/orcmaps-quality-delivery/tests/fixtures/tiny-gzip.pmtiles /mnt/f/AI/OrcSDR-TEMP/orcmaps-quality-delivery/examples/m5stack-tab5/test-pack/springfield-97477.manifest.json .local/maps/fit-order-b59d478-20261020/orcmaps_world-before.bin
cmake -S apps/orcsdr-tab5/tests/maps -B .local/maps/map-serial-asan-20261021 -DORCMAPS_ROOT=/mnt/f/AI/OrcSDR-TEMP/orcmaps-quality-delivery -DCMAKE_BUILD_TYPE=Debug "-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer" "-DCMAKE_C_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer"
cmake --build .local/maps/map-serial-asan-20261021 --target orcsdr_maps_tests -j2
ASAN_OPTIONS=detect_leaks=1 .local/maps/map-serial-asan-20261021/orcsdr_maps_tests /mnt/f/AI/OrcSDR-TEMP/orcmaps-quality-delivery/tests/fixtures/tiny-gzip.pmtiles /mnt/f/AI/OrcSDR-TEMP/orcmaps-quality-delivery/examples/m5stack-tab5/test-pack/springfield-97477.manifest.json
```

Final optimized and ASan/UBSan runs PASS. The real map screen, navigation,
preferences, camera and location persistence execute against explicit device
fakes. Tests cover auth rejection without state changes; malformed/oversized
input; proposal versus saved point; pan/zoom; failed save staying open; Cancel;
successful Save; independent labels and failed label persistence; pack lookup;
setup pause failure/Back/Quick/Finish/failure/restore; automatic progress write
failure; download unconfigured/installed/index/auth gates and cancellation.
They do not execute actual HTTPS or emulate RF/USB scheduling.

Logs in the invocation root:
`map-serial-final-host-{build,test}-20261021.log`,
`map-serial-final-sanitizer-{build,test}-20261021.log`.
Earlier configure/build logs retain the same `map-serial-*` prefix.

## Native checkpoint

```text
python .local/maps/build_fit_order_20261020.py build map-serial-20261021
python .local/maps/build_fit_order_20261020.py build map-serial-corrected-20261021
```

The first build stopped on three `-Werror=misleading-indentation` findings in
the shared actions. The formatting correction preserves behavior. The second
build passes. Logs: `fit-order-native-map-serial-20261021-20261020.log` and
`fit-order-native-map-serial-corrected-20261021-20261020.log`.
Artifact inspection, COM17 installation and command acceptance remain open at
this source checkpoint. COM39 is untouched.

## Initial COM17 command checks — `1cd9b180`

Native inspection passes; the dispatcher is linked and its 17,440-byte inventory
scratch is in `.ext_ram.bss`. App size 5,132,544 bytes, factory headroom 1,158,912
bytes. SHA-256 `6000efd9724e6012d9e778d6a7a48fe9bac88a2dd9012589a2297c4b9106db5b`;
ELF `aca3a0d832ac27daf0830b295777a79224793cf88ea6b415d47de1ef31ae30a9`.
Existing DMA reserve/symbol/PSRAM checks pass. App-only flash verifies COM17
30:ED:A0:E2:E6:95, previous app, fresh non-app backups and post-flash preservation.
Artifacts: `.local/maps/map-serial-1cd9b180-20261021/`.
Root logs: `map-serial-native-inspection-20261021.log`,
`map-serial-{verify-previous,backup-*,verify-backups,app-write,preservation}-20261021.log`.

Boot reaches SD/Wi-Fi/RTL ready and matches the ELF. Native monitor log is in its
invocation directory: `apps/orcsdr-tab5/map-serial-boot-native-monitor-20261021.log`.

Executed from the worktree root:

```text
python .local/maps/probe_map_serial_unauth_20261021.py
python .local/maps/com17_label_bench.py map-serial-device-plan-20261021.json map-serial-device-20261021.log
python .local/maps/com17_label_bench.py map-serial-save-setup-plan-20261021.json map-serial-save-setup-20261021.log
python .local/maps/com17_label_bench.py map-serial-save-setup-wait-plan-20261021.json map-serial-save-setup-wait-20261021.log
python .local/maps/com17_label_bench.py map-serial-settled-plan-20261021.json map-serial-settled-20261021.log
python .local/maps/com17_label_bench.py map-serial-keepalive-plan-20261021.json map-serial-keepalive-20261021.log
```

- Unauthenticated Help/Status succeed; opening a picker is rejected and maps
  stay inactive. The probe sends no reset/authentication/pairing command.
- Named picker open/zoom/pan/cancel PASS; the proposal moves but saved coordinates
  remain `444013775,-1229940563`. Return to ADS-B is confirmed.
- The first Save plan fails its **three-second observation window**, not an
  established firmware hang. A no-reset probe answers all commands, log
  `com17-hang-readonly-20261021-214722.log`. With a result-ended wait, Save
  verifies at 3.384 s and final acknowledgement arrives at 3.520 s. Coordinates
  are the original point; the test does not relocate the receiver.
- Setup reaches all five steps, with optional network/location/maps skipped.
  Driver is IDLE during setup. Finish succeeds; an immediate STREAMING assertion
  is too early for asynchronous restart, so that plan exits nonzero. A later
  settled query confirms STREAMING. This is not all-method setup acceptance.
- Label OFF/ON persistence checks pass. A later inventory Cancel fails when
  another screen owns the display. The user confirms they were using COM17 and
  switching to FT8; do not call that a spontaneous firmware navigation failure.
- Independently, the harness lacked heartbeat during long waits. The protocol
  requires PING within five seconds; normal command waits now send it every
  1.5 seconds. Binary transfer phases are unchanged. The old runner is preserved
  as `.local/maps/com17_label_bench_no_keepalive_20261021.py`. Earlier long-wait
  screen outcomes are inconclusive. No firmware timeout/workflow setting changed.
- A keepalive repeat PASSes inventory ownership, unsupported/unknown selection
  rejection, Cancel, ADS-B return and streaming. It also exposes a real rescan
  refresh defect: request generation changes 21→22, but the displayed frame stays
  at revision 8 after rescan. Discovery clears its cache without notifying the
  cached dashboard or ensuring a new request for an unchanged camera.

The follow-up source correction invalidates queued requests/preview lifetime
and presentation when discovery publishes metadata. It uses the existing atomic
cancel/invalidation operations while holding the publication lock.

### Rescan correction: COM17, `ba0e7a92`

Native build and inspection PASS:

```text
python .local/maps/build_fit_order_20261020.py build map-serial-rescan-20261021
C:/Espressif/python_env/idf5.5_py3.14_env/Scripts/python.exe .local/maps/inspect_map_serial_rescan_20261021.py
C:/Espressif/python_env/idf5.5_py3.14_env/Scripts/python.exe .local/maps/flash_map_serial_rescan_20261021.py
python .local/maps/com17_label_bench.py map-serial-rescan-plan-20261021.json map-serial-rescan-device-20261021.log
```

The app is 5,132,544 bytes, leaving 1,158,912 bytes in the factory partition.
SHA-256: `c92c9e84885cb59ae7d826599bd3a2ea12ddf69dcc2ff6bee288accf775b68cf`.
Inspection retains the 40 KiB DMA reserve, PSRAM placement and symbol gates.
Artifact/recovery directory: `.local/maps/map-serial-rescan-ba0e7a92-20261021/`.
Rollback app: `.local/maps/map-serial-1cd9b180-20261021/candidate.bin`.

Before flashing, the installed app was verified and reception stopped. Fresh
non-app partition backups were read and verified; post-write verification
confirmed app, bootloader, partition table and all backed-up partitions. Only
the P4 app was written. Native no-reset monitor captured one explicit USB reset
and the matching ELF digest; the pre-existing 6,080-byte core dump was preserved.
Wi-Fi initially retried, then connected and startup completed.

The heartbeat-enabled controlled plan exits **0**. Rescan now advances the
displayed frame revision **2 → 4** for the unchanged ADS-B camera. Labels OFF/ON
produce rendered label counts **2 → 0 → 2**, not just preference acknowledgements.
Inventory ownership, unsupported/unknown selection rejection, Cancel, return
to ADS-B and STREAMING checks pass. This tests the embedded overview; it does
not prove real global-pack installation or all dashboard consumers.

Logs, relative to the integration worktree:

- `fit-order-native-map-serial-rescan-20261021-20261020.log`
- `map-serial-rescan-inspection-20261021.log`
- `map-serial-rescan-preflash-20261021.log`
- `map-serial-rescan-verify-previous-20261021.log`,
  `map-serial-rescan-backup-*-20261021.log`,
  `map-serial-rescan-verify-backups-20261021.log`,
  `map-serial-rescan-app-write-20261021.log`,
  `map-serial-rescan-preservation-20261021.log`
- `apps/orcsdr-tab5/map-serial-rescan-boot-native-monitor-20261021.log`
- `map-serial-rescan-device-20261021.log`

During this bounded check, free PSRAM was 20,671,576 bytes, largest DMA block
8,704 bytes, DMA minimum 7,583 bytes and main-task stack high-water margin
3,996 bytes. These are observations, not sustained memory/RF acceptance.
No COM39 access occurred.

The corrected setup/save repeat also exits **0**:

```text
python .local/maps/com17_label_bench.py map-serial-rescan-setup-plan-20261021.json map-serial-rescan-setup-device-20261021.log
python .local/maps/com17_label_bench.py map-serial-return-ft8-plan-20261021.json map-serial-return-ft8-device-20261021.log
```

Save verifies the original coordinates; setup reports steps 0–4, IDLE during
setup and STREAMING after Finish settles. Optional network/location/maps pages
are skipped. Inventory rejection/Cancel and persisted ADS-B OFF/ON repeat pass.
COM17 returns to FT8, then the earlier 7,070,000 Hz tuning is restored and
confirmed. No serial process remains owning the port.

Final FT8 health reports 17,104,724 bytes free PSRAM, largest DMA block 8,704
bytes and main-task stack margin 3,500 bytes. The save/setup run reaches a DMA
minimum of **2,967 bytes**: this low transient margin remains an acceptance risk,
not a closed memory gate. Driver counters show zero overruns/drops in the final
short observation, which does not establish decoder/audio continuity or FT8 map
projection acceptance. Physical appearance was not newly checked in this run.

Remaining goal gates include actual global-pack/HTTPS delivery on device,
complete setup interruption/recovery, LoRa interaction, dashboard-specific
semantic controls (including ADS-B range), FT8 runtime, RF continuity and the
overall 33 ms target. No merge or publication occurred.
