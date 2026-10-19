# LoRa history validation — COM39, 2026-10-16

Follow-up NODEINFO testing found and corrected a cache bug: the decoder's
`encrypted` flag means encrypted on air, even when decryption succeeded with
the built-in public key. The original cache incorrectly discarded such decoded
names and positions. See `lora-nodeinfo-validation-2026-10-16.md` for the newer
firmware and follow-up results; the original measurements below remain historical.

Implementation and hardware persistence checks passed. This is an uncommitted
working branch, `codex/tab5-lora-history`, based on `origin/main` at
`bcae099d82c78d74a00501966d5d21d7c3237518`. Remote main was verified at that revision.
The approved, uncommitted 33 ms refresh and Home/RF Lab fixes were copied from
`tab5-home-33ms`; that original worktree is preserved. Nothing was pushed or merged.

## Hardware evidence

COM39: Tab5, RTL-SDR Blog V3c, user-confirmed LoRa antenna, US LongFast slot 20,
906.875 MHz. COM17 and COM40 were not accessed.

- Native ESP-IDF build passed. Application size: 4,616,928 bytes; SHA-256:
  `710a913dec2c63887b8c1b08f2759a7514e95def89f7bdf53b31a047627ae07a`.
- Application-only flash at `0x10000` completed with `Hash of data verified`.
- Automatic logging became ready after boot. During 150 seconds of live reception,
  five CRC-valid packets from five senders were archived. SD logging reported zero
  errors, zero queue drops, and no queued records after stopping/flushing.
- The decoder capture counter reported one drop; this is not a loss-free RF claim.
- CSV and JSONL were downloaded with the existing hash-verified SD transfer.
  CSV contained five records with the correct 18-column schema; JSONL contained
  five corresponding records and five distinct senders.
- Software reboot restored all five devices: `loaded=1 nodes=5 replayed=5
  rejected=0 written=0 error=0`. Each device query retained its UTC and packet count.
- Logging restarted automatically after reboot. COM39 was returned to the LoRa
  screen with logging enabled and native decoder ready.
- Device commands `UI_SELF_CHECK`, `RTL_UI_REGRESSION CHECK`, and
  `RTL_UI_REGRESSION RUN` passed all reported gates, including Home and RF Lab.

All five live records were marked encrypted; no receive key was loaded. Live
NODEINFO name/position persistence is therefore unverified. Name updates, missing
field preservation, and reboot reconstruction were tested with host records.
Hardware role/model, full protobuf payloads, and decrypted private-channel data
are outside this change.

## Exact commands and results

From `F:/AI/OrcSDR-TEMP/tab5-lora-history`:

```powershell
& ./apps/orcsdr-tab5/tools/build-tab5-idf.ps1
# PASS, exit 0; build-final.log records existing compiler warnings.

& C:/Espressif/python_env/idf5.5_py3.14_env/Scripts/python.exe -m esptool --chip esp32p4 -p COM39 --after hard_reset write_flash 0x10000 apps/orcsdr-tab5/build-native-hosted3/orcsdr_tab5.bin
# PASS, exit 0, flash-com39.log contains hash verification.

wsl -d Ubuntu -- bash /mnt/f/AI/OrcSDR-TEMP/tab5-lora-history/.local/lora-history/run-history-tests.sh
# PASS, exit 0, LORA_HISTORY_TESTS_PASS.

wsl -d Ubuntu -- bash /mnt/f/AI/OrcSDR-TEMP/tab5-lora-history/.local/lora-history/run-regression.sh
# PASS, exit 0: home_cadence, waterfall_style, focus_nav, home_vfo, band_profile.

python .local/lora-history/validate_live.py
# PASS, exit 0, live reception, logging stop/flush, file listing and hash readback.

python .local/lora-history/validate_reboot.py
# PASS, exit 0, READBACK_VALIDATED and REBOOT_REPLAY_PASS.

git diff --check
# PASS, exit 0; Git warns about inherited fm_dashboard.cpp line endings.
```

The host tests use AddressSanitizer and UndefinedBehaviorSanitizer with warnings
treated as errors. History tests cover duplicates, renames, absent fields,
coordinates, encrypted name rejection, host injection exclusion, 256-device
capacity, replay, malformed/truncated records, buffer limits, maximum-length
quoted fields, CSV quoting, and valid/unknown UTC filenames.

Evidence is retained under `.local/lora-history/`: build/flash logs, regression
logs, `live-com39.log`, `reboot-com39.log`, `ui-regression-com39.log`, read-back CSV
and JSONL, and the validation scripts. The preceding working 33 ms application
is preserved at `recovery/com39-working-33ms.bin`, SHA-256
`e31ae347b1c679ec48bd08ff4fa112ed06ca5c392587e47dd26d49d905b6d643`.

## Remaining acceptance boundaries

- Physical visual acceptance of the updated Nodes/Map screen is pending.
- Actual card-full, removal, and power-cut faults were not induced. Parser tests
  verify malformed-tail rejection, not FAT filesystem power-loss durability.
- Large, multi-day archives and long-term replay time remain untested. The archive
  is retained indefinitely; the working device cache is limited to 256 devices.
- This short LoRa test does not replace a long loaded soak. The earlier FM soak's
  final-minute audio-production decline and generic waterfall time-scale issue
  remain separate unresolved observations from the inherited refresh work.
