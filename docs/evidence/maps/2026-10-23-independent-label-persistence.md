# Independent map-label persistence — October 23, 2026

Result: native preference persistence PASS on COM17, USB serial
30:ED:A0:E2:E6:95. No firmware flash or source implementation change. The
controlled reboot's ELF prefix 2722a4f30 matches the saved f38b8601 ELF hash
2722a4f30ce0c9cb679c79ba77cbb753e4fd741fbe86cb558b496d194fa1c5f3.
This fingerprint comparison is not a new full flash readback.

## Procedure and actual results

Initial attempt stopped at the USB identity guard: COM17/reference serial was
absent from the inventory. No port opened and no preferences changed. The user
reconnected COM17; the subsequent identity check passed. COM39 was not opened.
Preserved failure: maps-labels-f38-before-console-20261023.log.

Read the original preference pattern and configured receiver point. Issued one
authenticated RTL_MAP LABELS action at a time and queried RTL_MAP STATUS after
each action. Each snapshot matched the expected changed bit with the other three
bits unchanged. All snapshots retained the same saved receiver coordinates.

| Stage | Picker | LoRa | ADS-B | FT8 |
|---|---:|---:|---:|---:|
| Original | 1 | 0 | 1 | 1 |
| Toggle Picker | 0 | 0 | 1 | 1 |
| Toggle LoRa | 0 | 1 | 1 | 1 |
| Toggle ADS-B | 0 | 1 | 0 | 1 |
| Toggle FT8 | 0 | 1 | 0 | 0 |
| After controlled reboot | 0 | 1 | 0 | 0 |
| Restored original | 1 | 0 | 1 | 1 |

Used the native IDF monitor with the matching saved ELF for one controlled reset,
then observed BOOT_STAGE ready and SPLASH_READY. Closed that monitor before
opening the command harness again. The status query after reboot loaded exactly
the changed pattern. Restored all four original values and verified their status
and the unchanged receiver point. NVS, SD/world and C6 were not erased/flashed;
only the four label settings were temporarily changed.

No explicit reception/decoder commands were issued. Initial observation was early
startup at 24,959 ms, receiver idle; normal startup later resumed LoRa reception.
Final sample: 906.875 MHz, about 957 kS/s, zero driver drops/overruns, 3,224,640
bytes free PSRAM/largest 3,211,264, largest DMA 19,456 and main-stack minimum
4,520. These are bounded startup/serial observations, not decoder acceptance or
map-load memory/performance measurements. No FT8 decoder test was performed.

## Commands and evidence

All commands run from F:/AI/OrcSDR-TEMP/maps-integration-completion. Logs, console
companions and JSON plans remain beside invocation. Authentication proofs are
redacted by the existing harness; no pairing key, Wi-Fi credential or unrelated
NVS data was extracted.

```text
python .local/maps/com17_label_bench.py maps-labels-f38-before-plan-20261023.json maps-labels-f38-before-connected-device-20261023.log
python .local/maps/prepare_label_persistence_f38_20261023.py
python .local/maps/com17_label_bench.py maps-labels-f38-toggle-plan-20261023.json maps-labels-f38-toggle-device-20261023.log
python .local/maps/com17_label_bench.py maps-labels-f38-before-plan-20261023.json maps-labels-f38-after-reboot-device-20261023.log
python .local/maps/com17_label_bench.py maps-labels-f38-restore-plan-20261023.json maps-labels-f38-restore-device-20261023.log
python .local/maps/verify_label_persistence_f38_20261023.py
```

All listed commands exit 0. Reboot capture:
maps-labels-f38-reboot-native-monitor-20261023.log; esp_idf_monitor with target
esp32p4, port COM17, 921600, matching candidate.elf, timestamps, coredump/panic
decoding disabled. USB serial was checked immediately before reset. Ctrl+] closed
the monitor cleanly with exit 0. No hardware process remains open.

Original/expected snapshot artifact: .local/maps/labels-f38-baseline-20261023.json.
Machine-checked result: maps-labels-f38-summary-20261023.json. The verifier checks
all four intermediate snapshots, reboot persistence, exact restoration, matching
boot ELF prefix and receiver-location retention.

## Acceptance boundary

Closes independent label preference persistence and named serial-setting coverage
on this candidate. UI and serial use the same preferences::set_labels handler;
the native test exercises named serial actions, not physical button/glyph
appearance on all four consumers. Those visual/projection requirements remain
separate. This does not close the failing two-second refill or cached-motion gate.
Producer 6a621a7 remains a source-only candidate awaiting its requested full-suite/
publication approval; COM17 remains f38b8601.
