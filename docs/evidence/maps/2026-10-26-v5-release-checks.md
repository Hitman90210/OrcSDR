# Nooelec V5 release checks - October 26, 2026

Scope: controlled off-air LoRa, stationary maps, navigation, setup pause/restore,
saved data and owner-accepted three-cycle hotplug recovery. This is not full
decoder or HF qualification; five recovery cycles were not performed.

## Bench

- COM17 USB `30:ED:A0:E2:E6:95`; owner confirmed Nooelec NESDR SMArt V5 and
  915 MHz antenna. Driver 0.9.3 identifies `nooelec_smart_v5_r820t2`, profile 3.
- The driver still reports `provisional=1` and `bias_cap=0`. No bias-enable
  operation was attempted; these tests do not remove the provisional flag or
  establish unsupported status for any untested decoder or HF path.
- App source `380e00ea`, SHA-256
  `3e1ae94a14319190fcb8932db0a4354e2a2026ab2f832e5b128d26a47eca6853`, C6 3.0.6.
- COM39 remains Blog V4L, 915 MHz antenna, app `00977434`, C6 1.4.1 unchanged.
  This is a reference reception check, not an isolated device comparison.
- Both receivers: US LongFast, slot 20, 906.875 MHz, 960,000 complex samples/s;
  RTL AGC and bias off. V5 reports automatic gain in the captured readbacks.
- Source: Heltec V4 on COM24, USB `44:1B:F6:6F:81:BC`, sender `aa745cf5`.
  Source configuration is checked before sending; every packet has hop limit
  zero. Sends are spaced 20 seconds apart, with submission UTC and packet IDs
  logged. No controlled sends occur during initial/final observation windows;
  independent mesh traffic can still be present.
- No firmware flash, C6 update, erase or source configuration change occurred.

## Actual results

| Check | Result |
|---|---|
| V5 identification and automatic resumed reception after swap | PASS |
| COM17 exact F1/F2/F3 text, sender and packet ID | PASS, 3/3 |
| COM17 exact NODEINFO sender and packet ID | PASS, 3/3 |
| COM39 reference text and NODEINFO | PASS, 3/3 each |
| Stationary reception IQ/driver counters | PASS, zero pipeline drops, overruns and driver drops on both |
| LoRa logger before/after RF | Ready, no new error; prior COM17 dropped count remains 9 |
| Saved text content, sender and packet ID | PASS, 3/3 in downloaded CSV |
| Saved NODEINFO short/long names, sender and packet ID | PASS, 3/3 in downloaded CSV |
| Label toggles and two picker pans | PASS, serial commands acknowledged |
| Setup entry pauses reception | PASS, driver IDLE |
| Setup Cancel resumes reception | PASS by explicit state readback |
| Retained-map scratch write/readback | PASS, 131,109 bytes exact |
| Final explicit stop | PASS, driver IDLE |

Stationary windows: 144,480 ms COM17 and 144,503 ms COM39. The COM17 world map
at the sampled viewport has one tile, 95 features, no missing/failed tiles and
555,720 frame bytes. Do not present these results as equivalent to the earlier
six-tile, 5,763-feature regional-view refill stress test.

Final COM17 memory readback: internal free 26,519 bytes, largest 14,848; DMA free
15,967, largest 14,848; PSRAM free 3,934,128, largest 2,883,584. Main stack high
watermark is 1,064 bytes. No new reset is observed; reset reason 11 remains from
the previously documented controlled coredump-readback reset.

Scratch SHA-256:
`25c778c7890a0ff22d421d998748751122564395c76f4555a963dc9b29cbbde8`.
The target and its `.part`/`.bak` siblings were checked absent before writing.
Existing user files were not replaced or removed.

The initial navigation harness exited on a missing literal `RTL_MAP_OK` during
Cancel. The captured line contains interleaved driver/serial output
(`=5RT1L_MA5P_OK`), followed by successful receiver initialization. The separate
follow-up explicitly verifies STREAMING and `RTL_MAP_VIEW active=0`, then
completes stop and storage checks. Preserve both logs; the initial script itself
did not pass. USB EP0 STALL messages during restart are also retained; the final
receiver state recovered and streamed. No unsupported-state relabeling is used.

The marked host storage diagnostic from the earlier V4 investigation is not
counted as RF evidence. Successful V5 logging does not recover the nine lost
V4 rows or resolve that earlier logging failure. The watchdog and intermittent
logger failure remain open in the V4 evidence record.

## Commands and evidence

Invocation directory: `F:/AI/OrcSDR-TEMP/maps-integration-completion`.
Raw logs, CSVs, scratch data and test manifests stay outside Git.

- `python .local/maps/heltec-v5-gate-send-20261026.py`
  - `release-v5-heltec-send-20261026.log`
- IDF Python `.local/maps/com17_label_bench.py .local/maps/v5-gate-rf-20261026.json release-v5-com17-rf-20261026.log`
- IDF Python `.local/maps/com39_keepalive_bench_20261026.py .local/maps/v5-gate-rf-20261026.json release-v5-reference-com39-rf-20261026.log`
- `python .local/maps/check_v5_gate_20261026.py`
  - Exit 0; both receivers report text 3/3 and NODEINFO 3/3.
  - Exact packet matches and log hashes: `.local/maps/release-v5-packet-results-20261026.json`.
- `release-v5-com17-navigation-sd-20261026.log` (fragmented acknowledgment attempt)
- `release-v5-com17-navigation-sd-followup-20261026.log` (completed readback/storage checks)
- `.local/maps/release-v5-com17-lora-saved-20261026.csv`
- `.local/maps/release-v5-sd-results-20261026.json`

## Owner-accepted three-cycle recovery

On October 26 the owner performed three V5 unplug/replug cycles and explicitly
accepted that scope instead of the original five: "i did it 3 times .thats good
enough. there have been no driver changes." No additional two cycles are required
for this accepted V5 checkpoint. No driver or firmware changes were made.

The monitor was running before the first unplug. For each cycle it captured
disconnection, then two STREAMING readbacks with increasing received-byte counts.
All three retained profile 3, 906.875 MHz, automatic gain, bias off and RTL AGC off.
Each recovered readback reported zero driver overruns and drops.

| Verified cycle | Disconnect to confirmed growing stream |
|---|---:|
| 1 | 9.637 s |
| 2 | 8.440 s |
| 3 | 16.878 s |

These observation intervals include the operator's unplug duration and polling;
they are not isolated driver enumeration or reconnect latency measurements.

The originally five-cycle monitor was stopped after the owner's instruction,
with three verified cycles and no disconnect pending. Its process was stopped
by the test controller; do not describe its original five-cycle program as
having completed successfully. The separate accepted-scope manifest records
the decision and the three observed cycles without inventing two more passes.

A subsequent Heltec packet (`OrcSDR V5 three-cycle recovery 20261026 G1`) matched
the exact sender, packet ID and text on both COM17 and the unchanged COM39
reference. Final COM17 hotplug state is ready/running with no resume pending;
LoRa logging is ready with no error and its prior dropped count remains 9.
Uptime increased from 3,270,119 to 3,599,032 ms, with no new reset observed.
COM17 remains receiving; no more unplugging is requested for this checkpoint.

Additional evidence beside invocation:

- `release-v5-com17-recovery-start-20261026.log`
- `release-v5-com17-five-cycle-recovery-20261026.log` (original filename retained)
- `.local/maps/release-v5-recovery-progress-20261026.json` (`STOPPED_BY_OWNER_AFTER_THREE`)
- `.local/maps/release-v5-recovery-accepted-three-20261026.json`
- `release-v5-post-recovery-heltec-send-20261026.log`
- `release-v5-com17-post-recovery-20261026.log`
- `release-v5-reference-com39-post-recovery-20261026.log`

The earlier watchdog reset and logging failure remain open. This accepted
recovery check does not complete other decoder rows or approve the release.
