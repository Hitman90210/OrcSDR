# Download feedback — COM17 native validation

Candidate source **c552111f**, engine **2133547bf8e003e2f4c52de3b8abf737261a8397**.
COM17 identity: **30:ED:A0:E2:E6:95**. COM39 was not accessed.
This installs the numeric verification fields and bounded header status shortcut;
it does not repeat the full archive benchmark or establish verification speed.

## Build and preservation

```text
python .local/maps/build_download_feedback_20261024.py build
python .local/maps/inspect_download_feedback_20261024.py
C:/Espressif/python_env/idf5.5_py3.14_env/Scripts/python.exe .local/maps/flash_download_feedback_20261024.py
C:/Espressif/python_env/idf5.5_py3.14_env/Scripts/python.exe .local/maps/monitor_download_feedback_20261024.py
```

All passed. The monitor captured the authorized startup reset, then was closed
before command tests. Build uses explicit saved SDKCONFIG, diagnostics OFF,
Hosted C6 3.0.6 image input and the existing TEST-ONLY catalog configuration.
The existing miniz symbol gate passes: no OrcMaps references to ROM miniz.

App: **5,287,744 bytes**, factory headroom **1,003,712 bytes**.
SHA-256: `fbffb4d5b2dc128051a64c813a1094f1cdf7d751677cafad15c8654f6a253a45`.
ELF: `a221dad1b301116a4a3d1ee733f757bd41245d3e49bcd925aac1afa717025bb5`.
The monitor reports the matching ELF prefix and SD/Wi-Fi/RTL ready. One initial
Wi-Fi connection failed with reason 2; the existing retry connected before Ready.
This is not a Wi-Fi regression investigation or qualification.

Link inspection places both the 152-byte header snapshot and 152-byte map-page
snapshot in `.ext_ram.bss`; existing map/replay/label/timing/FT8 allocations pass
their placement checks. The 40,960-byte internal reserve and 1,024-byte C++
emergency reserve remain unchanged. No full-size image buffer was added.

The previous app `137e244a` was byte-verified before writing. Fresh NVS, PHY,
coredump and embedded-world backups were saved and byte-verified afterward,
together with bootloader/table and the candidate app. No data-partition erase, SD file write,
world update or C6 update was performed by the flash procedure. Recovery and
artifact manifest: `.local/maps/maps-download-feedback-c552111f-20261024/`.

## Bounded device tests

```text
python .local/maps/com17_label_bench.py .local/maps/download-feedback-navigation-20261024.json maps-download-feedback-navigation-device-20261024.log
python .local/maps/com17_label_bench.py .local/maps/download-feedback-appearance-20261024.json maps-download-feedback-appearance-device-20261024.log
```

Both plans completed. They authenticate with the existing local key; no pairing
or credential replacement occurs. The first cancels a real catalog check,
observes `phase=3 busy=0`, then injects `RTL_UI TAP 360 40` through the normal
touch router. `RTL_MAP STATUS` confirms `downloads=1`. Closing the page and using
the authenticated named `RTL_MAP OPEN DOWNLOADS` reaches the same view. A later
catalog check reaches `phase=6 busy=0 ready=1 count=1`. No archive is downloaded.
Saved receiver coordinates match the pre-flash observation. The read-only
download status exposes the new numeric fields (all zero without a file scan).

After navigation: free PSRAM **5,158,092 bytes**, largest PSRAM block
**5,111,808 bytes**, largest internal DMA block **22,528 bytes**. Home is drawing
with `graphics=1`. LoRa reception at 906.875 MHz reports **958,368 samples/s**,
**0 drops / 0 overruns**. These are continuity checks, not decoder acceptance.
The appearance plan leaves Home with a deliberate cancelled-check `MAP PAUSED`
notice for the user's visual check; reception remains streaming (958,516
samples/s, 0 drops/overruns). Gain changed between observations, so no isolated
performance comparison is inferred.

## Evidence and open gates

Logs are saved beside invocation in the integration worktree:
`maps-download-feedback-native-build-20261024.log`,
`maps-download-feedback-inspection-20261024.log`,
`maps-download-feedback-flash-console-20261024.log`, its per-part flash logs,
`maps-download-feedback-boot-native-monitor-20261024.log`,
`maps-download-feedback-navigation-device-20261024.log`, and
`maps-download-feedback-appearance-device-20261024.log`, and
`maps-download-feedback-restore-device-20261024.log`.
Machine-checked results and log hashes:
`maps-download-feedback-validation-20261024.json`.

The user physically accepted the header notice: "Readable; controls remain clear."
Afterward the cancelled-check test state was cleared by a successful catalog
check. Final state: catalog ready, Home, reception 958,838 samples/s with zero
drops/overruns, serial port released. No archive transfer remains active. Real
full-file numeric verification progress and its overhead are not device-tested;
do not infer them from zero-valued status or the catalog check. No second 543 MB
download was started for this UI work. Retry counter continuity, concurrent RF
during HTTPS transfer, shared-worker map refill, the two-second detail-refill
gate, delivery failure cases and production decisions remain open.
