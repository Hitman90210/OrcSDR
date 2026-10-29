# Native-order immutable preview â€” COM17 experiment, October 19, 2026

Source aca08b75 changes only preview allocation byte-order enum; worker canvas,
orientation, locks and display ownership unchanged. Session authority covers the
native build and COM17 app-only bounded validation. No COM39 or C6 changes.

Host actual-M5GFX test PASS1/1 across all65536 values/eight sampling patterns,
P4 syntax PASS. See preview-native-order-source.md for exact commands/logs.

Native build command from integration root:

    python .local/maps/build_preview_format_20261019.py

Explicit saved IDF5.5.4/SDKCONFIG/C63.0.6 inputs and engine722d0d3 unchanged.
Build and inspection PASS. App5065040bytes, headroom1226416bytes;
appSHA256a5ed68c89fe832ecb5dab52a6a99c131d5992ad24e0fb289bc7abedbd00f4547.
ExternalBSS placement and configured DMA40960 unchanged. Pin/lock/miniz/FT8
gates pass. Artifact.local/maps/preview-format-aca08b75-20261019/ retained.

    python .local/maps/inspect_preview_format_20261019.py
    python .local/maps/flash_preview_format_20261019.py

App-only flash/preservation PASS. Previous3b4aae6d app verified, fresh allnonapp
backups verified, boot/table/NVS/phy/coredump/world preserved. TargetCOM17 serial
30:ED:A0:E2:E6:95 verified before each operation; C6notupdated, COM39untouched.
Logs preview-format-native-inspection-20261019.log, preview-format-flash-run-20261019.log
and each preview-format-{verify-previous,backup-*,verify-backups,app-write,preservation}-20261019.log.

Native monitor startup passed: SD/Wi-Fi/RTL ready. A transient Wi-Fi retry
recovered. No new panic/reset observed during bounded checks. Existing coredump
is historical. Monitor used no-reset entry, one explicit Ctrl+T Ctrl+R after
flash and Ctrl+] on exit, from apps/orcsdr-tab5. Log:
apps/orcsdr-tab5/preview-format-boot-native-monitor-20261019.log.

## Bounded device checks

From integration root:

    python .local/maps/com17_label_bench.py preview-format-postflash-ready-plan-20261019.json preview-format-postflash-ready-20261019.log
    python .local/maps/measure_preview_format_buttons_com17_20261019.py
    python .local/maps/measure_preview_format_zoom_com17_20261019.py
    python .local/maps/com17_label_bench.py preview-format-retained-read-plan-20261019.json preview-format-retained-read-20261019.log

All four complete plans/workloads PASS. The pan-only phase has 24 samples;
zoom has only three scaled previews. Later cumulative same-scale includes
zoom-return samples; table uses the pan-only snapshot.

| Path / metric | Samples | p50 ms | p95 ms | max ms |
| --- | ---: | ---: | ---: | ---: |

| Pan paint_call | 24 | 61.397 | 83.178 | 83.643 |
| Pan request_feedback | 24 | 61.671 | 83.396 | 83.846 |
| Pan clear_call | 24 | 0.013 | 16.759 | 17.463 |
| Pan transfer_call | 24 | 48.459 | 83.157 | 83.621 |
| Zoom paint_call | 3 | 232.754 | 233.155 | 233.155 |
| Zoom request_feedback | 3 | 233.122 | 233.394 | 233.394 |
| Zoom clear_call | 3 | 31.910 | 32.389 | 32.389 |
| Zoom transfer_call | 3 | 200.906 | 201.236 | 201.236 |

Runtime preview facts confirm source_depth=272 and target_depth=272, with
unchanged internal rotation3 and 1248×568 source. Direct rowmemcpy remains
unavailable at that rotation. The old checkpoint pan-only paint was68.716/
100.098ms p50/p95; current61.397/83.178ms. These bounded sequential runs show
lower costs, not a general percentage speedup or controlled all-workload result.
Zoom233ms is based on only three previews. Stage percentiles are independent,
not additive per-frame values. Presentation intervals contain dwell and scheduled
input spacing; no continuousFPS, physicaltouch or scanout claim. **33ms unmet.**
Publication conversion cost now occurs once on worker; separate publication
latency has not been quantified. Completed-frame push path is unchanged.

Radio before/after workloads STREAMING near2.4MSPS, zero observed driver drops/
overruns; C6 still3.0.6 match. Antenna suitability not independently verified;
no decoded-signal quality claim. SD existing8192byte checkpoint scratch readback
matched SHA256dc404a613fedaeb54034514bc6505f56b933caa5250299ba7d094377a51caa46.
Both map frames retained2835456bytes before/after. Reception paused for bulk
readback then resumed. No new scratch write, no old maps overwritten. Logs:
preview-format-buttons-20261019.log / preview-format-buttons-results-20261019.json,
preview-format-camera-20261019.log / preview-format-camera-results-20261019.json,
preview-format-retained-read-20261019.log (and corresponding *-run logs).

## Remaining validation

Native actual-library format test proves pixel-copy equivalence, including
sampling traversals. On October 19, the user confirmed the physical COM17
drag/zoom check: "Colors and controls look correct." This closes that visual
check; continuous-motion timing and worldwide map acceptance remain open.
Navigation plan also PASS:

    python .local/maps/com17_label_bench.py preview-format-navigation-plan-20261019.json preview-format-navigation-20261019.log

Home/RF Lab/Home and repeated Settings/Home returned graphics1; picker reopened
ready, C6match3.0.6. Device left receiving BROWSE26591754Hz on picker, serial
released. Latest health is in that log; memory/pairing-wide acceptance remains
open. Captured state does not prove absence of visual overlap. Retain
checkpoint210002f4 recovery. No new location writes or orientation changes.
Current change is provisionally retained for the observed lower costs, not
claimed as completed fluidity or worldwide integration. Global archive/device
acceptance, boundary styling/font coverage, trusted delivery/setup and final
regression/review/OrcSDRmain merge remain open.
