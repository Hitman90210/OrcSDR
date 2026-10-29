# Location save feedback — COM17 native checks, October 19, 2026

User approved this native cycle, then granted current-session routine build,
COM17 app-only flash and regression authority without per-cycle requests.
COM39/Claude and both C6 firmwares remain untouched. No erase/publication.

## Candidate / build / flash

Source `ea6a6b72` plus preserved dirty wizard and location-feedback edits.
Exact tracked diff and untracked wizard file hashes are sealed in
`.local/maps/location-feedback-ea6a6b72-20261019/manifest.json`.
Build ID `maps location feedback dirty 2026-10-19`.
App 5,062,976 bytes; remaining app partition 1,228,480 bytes.
App SHA-256 `568acd615a83dc300185092ef350912a0393930d2ba9af1217756fb394c99571`.
ELF SHA-256 `49d8f80d490ffe2322c6894e39639f50c808082654c115019c227cbf5e544bf8`.

Run from integration worktree root:

    python .local/maps/build_location_feedback_20261019.py
    python .local/maps/inspect_location_feedback_20261019.py
    python .local/maps/flash_location_feedback_20261019.py

One build passes, ESP-IDF 5.5.4 explicit saved configuration/C6 input;
diagnostics OFF, engine 722d0d3 pinned/locked/fetched. Native miniz/FT8 gates
pass, DMA reserve 40,960 bytes, exception reserve 1,024 bytes. Labels and all
preview/update timing pools remain in external BSS. No dependency lock change.
Build log apps/orcsdr-tab5/location-feedback-native-build-20261019.log;
inspection log location-feedback-native-inspection-20261019.log.

COM17 USB serial 30:ED:A0:E2:E6:95 verified before every esptool operation.
Previous strip app 5cad6710 verified; fresh NVS/phy/coredump/world backups
read/verified. App-only write at0x10000 and post-write boot/table/all non-app
preservation pass. Recovery and previous app retained. Flash logs:
location-feedback-flash-run-20261019.log and location-feedback-*-20261019.log.
The later panel-facts and editor-lifetime source edits are not in this artifact.

## Actual tests / results

Preflight saved live screenshot before any location writes. Original configured
point 44.7145048,-123.2137829, empty optional name, firmware-world map selected.
Screenshot .local/maps/location-feedback-before-20261019.bmp.
Command plans use the existing authenticated key without replacing pairing.
No Wi-Fi credentials extracted. Screenshots are uniquely named files on SD.

    python .local/maps/com17_label_bench.py location-feedback-preflight-plan-20261019.json location-feedback-preflight-20261019.log
    idf.py -B build-native-hosted3 -p COM17 monitor --no-reset
    python .local/maps/com17_label_bench.py location-feedback-open-plan-20261019.json location-feedback-open-20261019.log

App startup passed SD/Wi-Fi/RTL ready, C6 3.0.6 match. Native monitor was opened
from apps/orcsdr-tab5, one explicit Ctrl+T Ctrl+R booted the flashed app;
Ctrl+] released port. Log apps/orcsdr-tab5/location-feedback-boot-native-monitor-20261019.log.

First save-plan dispatch acknowledgement window was too short: after a tap,
no save/TAP_OK appeared within three seconds. Fresh probe showed responsive
command loop, streaming radio, no reset. A separate tap with a ten-second
window saved and returned successfully in about3.6 seconds. First plan did
not prove its intended editor transition: captured saved coordinates were
40.7139463,-121.1044079 rather than intended point. It is NOT counted as a
successful intended-point test or evidence of save corruption. Both failures
and probe logs are retained: location-feedback-save*, save-probe*, save-confirm*.

The resulting saved point survived a controlled native-monitor reboot:
location-feedback-reboot-native-monitor-20261019.log, reboot-check-plan/log.
Coordinate fields in saved/reboot BMPs match byte-for-byte (check_nav_location.py;
location-feedback-persistence-pixels-20261019.log). Both show SAVED LOCATION.
Original point restored via existing RTL_ADSB_LOCATION; initial two-second
ack window also expired, then a15-second bounded followup succeeded in3.6s.
Restored screenshot coordinates match the original byte-for-byte.

A gated repeat then proves exact edit/save behavior:

    python .local/maps/com17_label_bench.py location-feedback-editor-gate-plan-20261019.json location-feedback-editor-gate-20261019.log
    python .local/maps/com17_label_bench.py location-feedback-typed-gate-plan-20261019.json location-feedback-typed-gate-20261019.log
    python .local/maps/com17_label_bench.py location-feedback-proposal-gate-plan-20261019.json location-feedback-proposal-gate-20261019.log
    python .local/maps/com17_label_bench.py location-feedback-gated-save-plan-20261019.json location-feedback-gated-save-20261019.log

Inspected native-resolution captures before each dependent action: editor open,
typed447155048, proposed LAT447155048/LON-1232137829; then save verified and
Settings captured44.7155048,-123.2137829 with SAVED LOCATION. These exact gates
remove the earlier unverified dispatch assumption. No claim the earlier
mismatch's root cause was proven.

    python .local/maps/com17_label_bench.py location-feedback-final-restore-plan-20261019.json location-feedback-final-restore-20261019.log
    python .local/maps/check_nav_location.py .local/maps/location-feedback-before-20261019.bmp .local/maps/location-feedback-final-restored-20261019.bmp same

Final original point restored; field-pixel hash matches before, log
location-feedback-final-restored-pixels-20261019.log. Reception restored BROWSE
26,591,754Hz near2.4MSPS, zero observed driver drops/overruns, C6match3.0.6.
Screenshots/8KiB test files remain, no existing packs overwritten. No new panic
or reset observed except explicit flash/reboot controls. Serial released.

## Limits / next gates

Host tests prove SAVING drawing is invoked before storage; native save and
returned SAVED LOCATION accepted in captures. The user subsequently confirmed physical acceptance: “Saving feedback and
saved label look good”. The verified3.6s synchronous-save
cost remains; this UI patch does not claim to remove that delay. Active-radio
continuity is bounded stream-counter evidence, not RF decoding quality.

A narrow review found an abandoned setup editor could remain active after
external navigation. A separate ownership-aware teardown source correction
and read-only preview panel diagnostic are prepared for the next native cycle;
not included here. Global pack acceptance, geographic quality, trusted catalog/
download setup,33ms transfer work and final integration remain open.
