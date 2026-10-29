# Blog V4 release checks - October 26, 2026

Status: RF retry passed; storage and stability remain open. A watchdog reset
interrupted the first COM17 attempt. Do not count that attempt as a completed
decoder or recovery qualification.

## Identity and setup

- COM17 USB `30:ED:A0:E2:E6:95`; owner confirmed Blog V4 and 915 MHz antenna.
- Driver 0.9.3 identifies `blog_v4_r828d`, profile 1, non-provisional.
- App source `380e00ea`; app SHA-256
  `3e1ae94a14319190fcb8932db0a4354e2a2026ab2f832e5b128d26a47eca6853`.
- C6 3.0.6; no C6 flash or settings erase.
- US LongFast, slot 20, 906.875 MHz, 960,000 complex samples/s; bias and RTL AGC off.
- COM39 remains the Blog V4L/915 MHz reference on source `00977434`, C6 1.4.1.
  Its app is different, so this is not an isolated dongle performance comparison.
- Source: Heltec V4 COM24, USB `44:1B:F6:6F:81:BC`, sender `aa745cf5`.
  Readback guards region/preset/slot/transmitter state before each run; no source
  settings are changed. Every controlled packet has hop limit zero.
- Off-air test, not a measured conducted RF or HF-path qualification. Automatic
  receiver gain-guard changes are preserved in the logs.

## Initial recovery and watchdog incident

COM17's initial readback showed the correct V4 profile, streaming near
958,370 samples/s, with zero driver overruns/drops. Its next recording attempt
timed out writing `RTL_STATUS`, before its UI/map test steps executed.
A passive, no-reset serial capture then received `RTL_RESET_REASON code=7
name=wdt`. The following health readback confirmed reset reason 7 and uptime
67,922 ms. Serial evidence also contained C6 link-loss/recovery activity before
the reset, but no root cause is established. Do not attribute it to the V4,
LoRa decoder, map renderer, or Wi-Fi solely from that observation.

The controlled D-series source started sending at 19:42:56 UTC. COM17's reboot
timing inferred from the later uptime predates that first controlled send.
The source continued and COM39 matched all three D-series text IDs/contents and
all three controlled NODEINFO IDs. COM17 recovered and later matched D2, but its
interrupted session is not a three-repetition pass.

## Preserved dump and controlled readback reset

The coredump partition was read once without writing flash. This operation and
returning from the bootloader intentionally reset COM17; distinguish those
controlled resets from the earlier watchdog reset.

Dump: `.local/maps/release-v4-com17-coredump-20261026.bin`, 262,144 bytes,
SHA-256 `68a19d94cf082dd0680c8d872cdd2a877c960776409641945ca55a42c968b3ca`.
The digest is identical to both the pre-00977434 and pre-380e00ea backups. It is
an older retained dump, not new evidence of today's watchdog failure.
`esp_coredump info_corefile` correctly rejects it against the current ELF:
`coredump SHA256(b2cd1a716) != app SHA256(5b93298f1)`.
No attempt was made to force a mismatched decode or use that stale stack as the
cause of today's incident. Raw dump contents stay outside Git.

After the controlled reset, COM17 booted, identified the V4 and resumed LoRa.
A fresh packet test is run separately; even a successful retry does not erase
the open watchdog observation.

## Fresh retry and navigation results

The E-series retry matches all three text packets and all three NODEINFO packets
on both COM17 V4 and COM39 V4L. Matching checks sender `aa745cf5`, packet ID,
port and exact text. Expected NODEINFO names are also present in node-history
readback. No host injection is used for these RF results. The Heltec script
spaces every controlled transmission by 20 seconds and records submission UTC;
independent mesh traffic can still occur during the no-controlled-send windows.

Command: `python .local/maps/check_v4_gate_20261026.py`.
Actual result: `COM17 texts 3 /3; NODEINFO 3 /3; plan complete True` and the same
for COM39. Packet evidence and log digests are in
`.local/maps/release-v4-packet-results-20261026.json`.
Both stationary reception windows reported zero IQ pipeline drops and zero
driver overruns/drops (144,612 ms COM17, 144,804 ms COM39). No additional reset
was observed during this retry. The two map viewports differ in detail density;
these numbers do not establish comparative rendering performance.

COM17 subsequently passed label toggles, two picker pans, return to LoRa,
setup reception pause (driver IDLE), Cancel restoring STREAMING, and explicit
reception stop. Its saved receiver coordinates remained unchanged. These are
serial-state checks, not a new physical appearance or audio acceptance.
The owner separately confirmed that the screen responds normally after recovery.

## Storage failure and bounded recovery diagnostic

The V4 retry's logging status was `requested=true ready=false error=true queued=0
dropped=9`. All three E-series texts are absent from the downloaded CSV. This is
a failed persistence result even though their live RF reception passed.

With reception stopped and map memory retained, a uniquely named 131,109-byte
scratch file wrote and read back byte-for-byte (SHA-256
`25c778c7890a0ff22d421d998748751122564395c76f4555a963dc9b29cbbde8`). The helper
first proved the target and its `.part`/`.bak` siblings absent. The CSV readback
also completed with the file-transfer checksum verified. Thus the log failure
is not evidence that all SD access was unavailable; the failing logger stage
and filesystem error were not exposed by the existing status command.

After recording that failure, named command `LORA_SD_LOG ON` cleared the latched
error and reopened logging. A single `LORA_MESSAGE` storage diagnostic, explicitly
marked host-origin, sender `fffe0001`, packet ID `26102601`, text
`HOST SD DIAGNOSTIC 20261026 A`, then persisted with `from_host=1`. This is a
storage probe, not a decoder pass. Ready became true, error false and queued
zero; the prior dropped count remained 9. No old data was removed or renamed.

This manual recovery does not explain the original failure, recover the missing
E-series records, or qualify unattended retry behavior. No firmware workaround
has been applied. Keep logger persistence and the watchdog incident open before
the overall release decision. COM17 remains stopped for the next dongle checkpoint.

## Logs beside invocation

Invocation directory: `F:/AI/OrcSDR-TEMP/maps-integration-completion`.

- `release-v4-com17-preflight-20261026.log`
- `release-v4-com17-rf-20261026.log` and `release-v4-com17-rf-console-20261026.log`
- `release-v4-com17-passive-after-timeout-20261026.log`
- `release-v4-com17-timeout-readback-20261026.log`
- `release-v4-com17-watchdog-native-monitor-20261026.log` (no-reset monitor)
- `release-v4-com17-post-watchdog-20261026.log`
- `release-v4-com17-coredump-read-20261026.log`
- `release-v4-com17-coredump-decoded-20261026.log` (rejected stale ELF identity)
- `release-v4-com17-after-dump-boot-20261026.log`
- `release-v4-com17-after-dump-readback-20261026.log`
- `release-v4-reference-com39-rf-20261026.log`
- `release-v4-heltec-send-20261026.log`
- `.local/maps/release-v4-reference-results-20261026.json`
- `release-v4-{com17,reference-com39}-rf-retry-20261026.log`
- `release-v4-heltec-retry-send-20261026.log`
- `release-v4-com17-navigation-sd-20261026.log`
- `.local/maps/release-v4-sd-results-20261026.json` (failed saved-text checks)
- `release-v4-com17-sd-diagnose-20261026.log`
- `release-v4-com17-sd-host-diagnostic-20261026.log`
- `.local/maps/release-v4-com17-lora-host-diagnostic-20261026.csv`

Required protocol rows, five unplug/replug cycles, physical appearance/audio,
HF and remaining dongles are not qualified by this partial record.
