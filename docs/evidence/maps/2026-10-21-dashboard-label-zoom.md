# Dashboard label zoom and ADS-B range — October 21, 2026

COM17 (USB serial 30:ED:A0:E2:E6:95) runs the app built from `4b22585b`.
The reported Labels button defect was reproduced without resetting it:
authenticated taps at (110,276) changed only ADS-B's stored label preference
from 1 to 0 to 1, while every completed frame reported zero labels.
Log: `label-toggle-probe-20261021.log` in the OrcSDR worktree root.

The shared pipeline used the embedded archive's adapted source zoom (at most
4) for the city-name threshold (6). The correction passes the requested camera
zoom separately for label eligibility and priority, retaining the source camera
for tile lookup and projection. Legacy fallback also respects the threshold.
Labels OFF still supplies no label layout/text target. This does not add places
absent from the embedded overview or install the global candidate.

Focused checks, executed from the integration worktree root:

```text
cmake --build .local/maps/labels-host --target orcsdr_map_preferences_tests orcsdr_maps_tests -j2
.local/maps/labels-host/orcsdr_map_preferences_tests
.local/maps/labels-host/orcsdr_maps_tests /mnt/f/AI/OrcSDR-TEMP/orcmaps-quality-delivery/tests/fixtures/tiny-gzip.pmtiles /mnt/f/AI/OrcSDR-TEMP/orcmaps-quality-delivery/examples/m5stack-tab5/test-pack/springfield-97477.manifest.json .local/maps/fit-order-b59d478-20261020/orcmaps_world-before.bin
```

These Linux commands ran through WSL. Preferences PASS; map screen, world and
pipeline checks PASS. The actual backed-up COM17 world archive renders London
and Salem at logical zoom 6 / source zoom 4; coarse views remain unlabelled.
Logs: `label-toggle-host-build-20261021.log`,
`label-toggle-preferences-20261021.log`, `label-toggle-world-20261021.log`.
An additional explicit OFF render is included in the world regression;
its rerun is recorded separately in `label-toggle-off-*-20261021.log`.

The prepared ADS-B default is 10 NM on the short axis, approximately 47 statute
miles across the wide map. Existing valid saved ranges are preserved. Disabled
zoom endpoints are dimmed. Receiver centering continues to use saved lat/lon.

Native build, installation and corrected physical behavior were unverified
at this source checkpoint; the continuation below records later automated gates.
COM39 is untouched.

The earlier apparent hang was withdrawn by the user. The no-reset read-only
probe answered all five commands, with uptime 54,613,749 ms (about 15.2 hours),
20,637,308 bytes free PSRAM and an 8,704-byte largest DMA block. This is not a
controlled soak or a confirmed hang. Log:
`com17-hang-readonly-20261021-135603.log`.

## Native and COM17 continuation

Source commit: `b694208a` (nine firmware/test files listed in that commit, this
evidence note and the execution plan). The focused OFF-render rerun passes.

Executed from the OrcSDR worktree root:

```text
python .local/maps/build_fit_order_20261020.py build logical-label-20261021
C:/Espressif/python_env/idf5.5_py3.14_env/Scripts/python.exe .local/maps/inspect_logical_label_20261021.py
python .local/maps/com17_label_bench.py logical-label-preflash-plan-20261021.json logical-label-preflash-20261021.log
python .local/maps/flash_logical_label_20261021.py
python .local/maps/com17_label_bench.py logical-label-device-plan-20261021.json logical-label-device-20261021.log
python .local/maps/com17_label_bench.py logical-label-range-plan-20261021.json logical-label-range-20261021.log
python .local/maps/com17_label_bench.py logical-label-wide-plan-20261021.json logical-label-wide-20261021.log
```

All return exit 0. Native build log:
`fit-order-native-logical-label-20261021-20261020.log` (the reused runner retains
its historical suffix). Inspection log: `logical-label-native-inspection-20261021.log`.
Artifact: `.local/maps/logical-label-b694208a-20261021/`, with manifest, ELF,
app and fresh non-app backups. App size 5,130,976 bytes; factory headroom
1,160,480 bytes. App SHA-256:
`2c17269df37fe51aef1a51ce9bfde998c860d026e7cd0a31380e209e3862e056`.
The 40 KiB internal DMA reserve and checked PSRAM placements are preserved.

The flash script verifies COM17 identity and the previous app, backs up and
verifies non-app partitions, writes only the P4 app, then verifies app, original
bootloader/table and all backed-up partitions. All gates PASS; logs are root
`logical-label-{verify-previous,backup-*,verify-backups,app-write,preservation}-20261021.log`.
No SD/world/NVS/C6 erasure or C6 update occurred.

Startup was observed with `idf.py -B build-native-hosted3 -p COM17 monitor
--no-reset`, then one deliberate Ctrl-T/Ctrl-R reset. Saved in the monitor's
invocation directory: `apps/orcsdr-tab5/logical-label-boot-native-monitor-20261021.log`.
ELF hash prefix `994e1baa0` matches the artifact; SD, Wi-Fi and RTL reached ready.
The preserved 6,080-byte old core dump is not evidence of a new crash.

At the closer test ranges, 13 eligible archive places were all offscreen. At
100 NM, the completed map reports **ON: offered 13/drawn 2; OFF: offered 0/drawn
0; ON again: offered 13/drawn 2**. Other consumers' preferences remain 1.
COM17 was left at 100 NM with Labels ON for physical inspection. These tests
confirm shared render eligibility and button routing, not close-area data
coverage. The embedded overview lacks the local density of the global candidate.

Bounded post-update driver check: STREAMING, approximately 2.048 MSPS,
zero driver drops/overruns; free PSRAM 20,685,384 bytes, largest DMA block
9,216 bytes. This does not close the separately observed decoder-queue losses.
Physical ON/OFF confirmation is pending. Next milestone: resolve map coverage
selection and validate the regional/global pack on device without substituting
legacy names over a different projection.
