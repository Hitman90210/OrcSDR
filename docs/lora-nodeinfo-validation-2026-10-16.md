# NODEINFO and position persistence follow-up — COM39

The controlled-transmitter gap described below was subsequently closed after the
user identified COM24. See `controlled-heltec-validation-2026-10-16.md` for the
successful Heltec NODEINFO/position, SD packet-ID and reboot checks.

The follow-up found and fixed a real cache bug. The decoder's `encrypted` field
means that encryption was used on air, including payloads successfully decoded
with the built-in public channel key. The new history cache had incorrectly used
that flag to reject names and positions. Two real NODEINFO names were present in
the CSV while their cached device names were empty. Removing that incorrect guard
allows the existing decoded fields to be retained. Undecoded frames have no name
or valid position and cannot erase saved fields. Host injections remain excluded.

No decoder, radio settings, receive keys, or protocol handling were changed.
The existing archive format is compatible: replay recovers previously saved names
and coordinates without modifying old records. The usage guide now explains the
encryption flag correctly; a missing custom receive key does not prevent use of
the built-in public channel key.

## Hardware results

- COM39, V3c, LoRa antenna, US LongFast slot 20, 906.875 MHz.
- Before the fix: 70 saved RF records included two named NODEINFO records and
  32 position reports from 11 devices. Both named device cache queries were empty.
- After the fix: all saved names and positions matched an independent fold of
  the downloaded CSV: two named devices, 11 positioned devices, 13 devices checked.
- A further 100-second reception window saved one new CRC-valid port-67 packet.
  SD status reported zero errors, zero queue drops; the decoder capture drop
  counter reported zero during this window.
- After another software reboot: all 71 records replayed, zero rejected, 25
  devices retained. Both names and all 11 retained coordinates matched the CSV.
- CSV download was hash verified. SHA-256:
  `94651f14d11631229e3f1db33e3a49f71f26a1228c234b5cfe6d97739e46b8af`.
- COM39 was returned to LoRa with automatic logging enabled, ready, and error-free.

These checks use real received RF data, not host-injected device entries.
The named and position records were received before the fix and verified through
archive replay afterward. The additional post-fix packet was neither NODEINFO nor
position. A fresh controlled NODEINFO/position transmission remains untested.

## Transmitter identification boundary

COM38 responded `SXERR command=unknown` to read-only command probes and did not
respond to the Meshtastic API. Its identity as the attached Heltec was not
confirmed. COM30 did not respond to a read-only status probe. No transmitter was
reconfigured, flashed, or instructed to transmit. COM17 and COM40 were untouched.
The controlled Heltec test needs its current port and firmware command interface.

## Build and tests

Application-only build and flash passed; app SHA-256:
`9ceb2c01bf56acce52cceb6546b331d3476c7e227e066a0e2cc75c82a5a20e41`.
Flash reported `Hash of data verified`. Pre-fix application was preserved under
`.local/lora-nodeinfo/com39-before-nodeinfo-fix.bin`, SHA-256
`710a913dec2c63887b8c1b08f2759a7514e95def89f7bdf53b31a047627ae07a`.

Exact commands from `F:/AI/OrcSDR-TEMP/tab5-lora-history`:

```powershell
wsl -d Ubuntu -- bash /mnt/f/AI/OrcSDR-TEMP/tab5-lora-history/.local/lora-history/run-history-tests.sh
# PASS, exit 0, LORA_HISTORY_TESTS_PASS; ASan/UBSan, warnings as errors.

& ./apps/orcsdr-tab5/tools/build-tab5-idf.ps1
# PASS, exit 0.

& C:/Espressif/python_env/idf5.5_py3.14_env/Scripts/python.exe -m esptool --chip esp32p4 -p COM39 --after hard_reset write_flash 0x10000 apps/orcsdr-tab5/build-native-hosted3/orcsdr_tab5.bin
# PASS, exit 0, application hash verified.

python .local/lora-nodeinfo/test_live.py
# LIVE_HISTORY_PASS and REBOOT_HISTORY_PASS.
# Overall exit 1 because a subsequent UI RUN restoration assertion failed.

git diff --check
# PASS, exit 0; inherited fm_dashboard.cpp line-ending warning only.
```

The host test now explicitly covers encrypted-on-air decoded NODEINFO and valid
coordinates, plus absent-field preservation for an undecoded encrypted frame.

`UI_SELF_CHECK` passed all reported gates. `RTL_UI_REGRESSION CHECK` passed.
The first `RTL_UI_REGRESSION RUN`, from the post-boot startup Home state, reported
`restored=0` with every other gate passing. After explicitly opening Home, both
CHECK and RUN passed with `restored=1`. The initial failure is retained, not
discarded. Source inspection suggests the RUN workflow changes the initially
inactive radio UI state; that startup-state restoration limitation was not fixed
in this narrow cache change.

Evidence: `.local/lora-nodeinfo/` contains `before.csv`, `after.csv`, build/flash
logs, `history-tests.log`, `live-test.log`, `ui-settled.log`, and validation scripts.
The work remains uncommitted on `codex/tab5-lora-history`; nothing was pushed or
merged. Physical Nodes/Map visual acceptance remains separate from these cache
and persistence checks.
