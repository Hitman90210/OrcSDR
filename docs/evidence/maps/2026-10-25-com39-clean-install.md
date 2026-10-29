# COM39 clean-install check

User explicitly authorized a COM39 P4 erase to simulate first installation and
later waived a new firmware backup. Target identity was checked before commands:
COM39, USB serial `E8:F6:0A:E2:D6:03`. COM17 was not accessed during this test.

## Installed artifact and erase boundary

- Source `6f625f66e5b2227fbb57189b1a896be91d458f03`.
- App SHA-256 `b314ac3e1ba8e70158e53a3d49f00a18162d2914f2d282723e4a05cafe09de31`.
- P4 flash fully erased, then bootloader at 0x2000, partition table at 0x8000,
  inspected app at 0x10000 and reviewed embedded world at 0x650000 installed.
- Bootloader/table came from the recovery copies already verified with this
  app on COM17. The current rebuilt bootloader was not substituted silently.
- World archive 871,343 bytes, SHA-256
  `9aea08772bacf1f024d1da90cc52aa8fcf0b0e37405415dc7c91e75e56596f0f`.
- Existing P4 NVS/settings were intentionally cleared. No SD erase or pack
  removal was performed. Ordinary firmware SD startup/logging is not disabled.
- C6 was not flashed; read-only status before and after reports **1.4.1**.
  The P4 includes an optional 3.0.6 image, but no C6-update command was issued.

## Actual results

Two full-backup attempts lost serial access around 2.17 MB. The user noted a
possible manual reset, then waived backup. Several subsequent opens were denied
by Windows; all this session's backup/monitor/flash processes had exited. A
read-only handle diagnostic did not identify an owner and was stopped. No other
session's processes were killed. No erase had occurred at those failures.

After the user unplugged/reconnected, COM39 identity and open/close checks passed.
Erase succeeded in 7.5 seconds; write and explicit verification of all four
regions passed. Startup reached Ready with SD and receiver available, Wi-Fi off.
The untouched C6 later initialized transport and reported 1.4.1.

Read-only queries at 21:47 local, 2026-10-25:

- `RTL_MAP_VIEW active=1 mode=2 step=1`: setup wizard active, Wi-Fi step.
- `RTL_MAP_SAVED configured=0`: no stored receiver location after erase.
- Driver 0.9.3, Blog V4L, IDLE, bias off: reception paused during setup.
- Free PSRAM 22,519,748 bytes; largest PSRAM block 22,020,096 bytes.
- C6 transport ready, coprocessor 1.4.1, optional update not applied.

The automated checks did not send setup Next/Skip/Finish, pair a new serial key,
save location or connect Wi-Fi. The user may interact with the screen; observed
step 1 does not prove which action advanced Welcome. Physical screen acceptance
and the complete interactive first-boot workflow remain for the user to exercise.
This is a direct clean P4 installation, not a published M5Burner package test.

## Saved evidence

Run directory: `F:\AI\OrcSDR-TEMP\maps-integration-completion`. Logs/artifacts stay
there outside Git:

- `com39-clean-install-preflight-device-20261025.log`
- `com39-clean-install-{backup,retry-backup,retry2-backup}-20261025.log`
- `python .local/maps/flash_com39_firstboot_reconnected_20261025.py` — PASS;
  `com39-firstboot-reconnected-{erase,write,verify}-20261025.log`.
- `.local/maps/com39-firstboot-6f625f66-20261025/manifest.json` and frozen files.
- `com39-firstboot-native-monitor-20261025.log` — startup capture; closed.
- `python .local/maps/com39_bench.py .local/maps/com39-firstboot-readonly-plan-20261025.json com39-firstboot-readonly-device-20261025.log` — completed.

Next milestone: user walks through setup, then verify chosen location, map
availability and setup completion persist across restart without changing C6.
