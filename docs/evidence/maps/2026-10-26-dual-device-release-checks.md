# Parallel Tab5 release checks - October 26, 2026

This is a bounded hardware record, not a completed four-dongle decoder gate.
Raw logs and downloaded evidence remain outside Git beside the script invocation
in `F:/AI/OrcSDR-TEMP/maps-integration-completion`.

## Bench identity

| Device | USB serial | Dongle | App source | App SHA-256 | C6 |
|---|---|---|---|---|---|
| COM17 | 30:ED:A0:E2:E6:95 | Blog V3c, `blog_v3_r820t2`, profile 2 | 380e00ea | 3e1ae94a14319190fcb8932db0a4354e2a2026ab2f832e5b128d26a47eca6853 | 3.0.6 |
| COM39 | E8:F6:0A:E2:D6:03 | Blog V4L, `blog_v4l_r828s`, profile 4 | 00977434 | 3516cadd31610cb5341334312f75e3fdee3f1906016c61b0d697fe364544f4bd | 1.4.1 preserved |

Driver: esp_rtl_sdr 0.9.3. Both receivers use US LongFast, slot 20,
906.875 MHz, 960,000 complex samples/s, RTL AGC off and bias off.
The source is Heltec V4 on COM24, USB 44:1B:F6:6F:81:BC, sender `aa745cf5`.
Its region, modem preset, automatic slot and enabled transmitter were read back
before transmission. Packets use hop limit zero and do not request acknowledgments.

The owner confirmed a 915 MHz antenna on COM39. COM17 initially had an MLA30+;
reception was stopped and the owner confirmed replacement with a 915 MHz antenna
before its controlled packet qualification. No conducted RF, measured level,
Pluto calibration or HF-path qualification is claimed. Both automatic gain guards
responded to the strong nearby signal; final gain is recorded in the serial logs.

The apps differ by the later SD update-mode correction. These are separate test
environments, not an isolated dongle or C6 performance comparison.
The existing local automation key was accepted on COM39 using the firmware's
non-replacing PAIR operation; a different existing key cannot be overwritten.
No C6 update, erase, location change or map-pack replacement was performed.

## Completed baseline and storage checks

| Check | COM17 V3c | COM39 V4L |
|---|---|---|
| Three controlled texts, exact sender, packet ID and text | PASS, A1/A2/A3 | PASS, A1/A2/A3 |
| Initial controlled NODEINFO | Not scored: receiver restart overlapped transmission | PASS |
| Baseline driver overruns/drops | 0/0 | 0/0 |
| LoRa SD logging status | Ready, no error or dropped queued rows | Ready, no error or dropped queued rows |
| Baseline A1/A2/A3 found in SD file readback | PASS | PASS |
| Retained-map scratch write/readback | PASS, 131,109 bytes exact | PASS, 131,109 bytes exact |
| Reception restored after storage checks | PASS | PASS |

Scratch files have unique names and were checked absent, along with `.part` and
`.bak` siblings, before writing. Their SHA-256 is
`25c778c7890a0ff22d421d998748751122564395c76f4555a963dc9b29cbbde8`.
Receiver data was not overwritten. The file-transfer protocol verified the
scratch and saved-log readbacks; the host additionally compared scratch bytes.

Logs: `release-dual-{com17,com39}-{preflight,prepare,lora-capture,sd}-20261026.log`.
COM17's antenna-corrected preparation is
`release-dual-com17-915-prepare-20261026.log`.
Source packet identities are in `release-dual-heltec-send-20261026.log`.
Saved files and exact-match results are under `.local/maps/release-dual-*` and
`.local/maps/dual-gate-packet-results-20261026.json`.

## Map-load observation

COM17 completed six full-screen picker pans and label changes, then returned to
the LoRa map without reset or SD errors. It did not receive the three B-series
texts sent during repeated detail refills. Only one of four controlled NODEINFO
packets from that phase was observed. This is not a decoder-under-load pass.
Its cumulative IQ pipeline drops rose from 150 to 2,858 and driver overruns/drops
from 0 to 93. After settling, a 13,116 ms window left those counters unchanged,
with no backlog and approximately 959,355 samples/s processed.

COM39 received all B-series texts and four NODEINFO packets, but its first map
control attempt expired the harness authentication session. The keepalive-corrected
retry completed controls, with zero pipeline and driver drops. Its map was still
busy at one return sample, so packet reception during the earlier attempt must
not be described as proof of equivalent render load or superior dongle performance.

The owner accepted small transient map-interaction IQ drops. Preserve this packet
loss result explicitly rather than relabeling it as a zero-loss or full load pass.
Stationary-map reception is checked separately before the next dongle swap.

Logs: `release-dual-com17-map-rf-20261026.log`,
`release-dual-com39-map-rf-20261026.log` (failed harness attempt),
`release-dual-com39-map-rf-retry-20261026.log`, and
`release-dual-map-heltec-send-20261026.log`.

## Stationary map follow-up

Both devices subsequently reported a ready regional map at camera zoom 9,
source zoom 8, six tiles, 5,763 features, no missing/failed tiles and 555,720 frame
bytes. The previous COM39 busy sample therefore does not imply a permanently
missing map.

Both receivers matched all four new controlled NODEINFO IDs, and the history
readback contains the expected `hrdc` / `Hardcoreerik` names. Both matched C1 and
C3 text IDs and contents; neither matched C2. A Meshtastic API submission alone
does not prove that C2 was successfully transmitted over RF. This attempt is
not a three-of-three stationary text pass and its cause remains unassigned.
The independent baseline three-of-three text result above remains valid.

Stationary windows were 82,666 ms (COM17) and 82,410 ms (COM39), with zero IQ
pipeline drops, driver overruns and driver drops in both windows. Those counters
were reset by the intentional storage-check stop/start, so do not compare them
as lifetime totals with the earlier map-load counters.

Logs: `release-dual-{com17,com39}-stationary-rf-20261026.log`,
`release-dual-{com17,com39}-final-20261026.log`, and
`release-dual-stationary-heltec-send-20261026.log`.
Exact packet results: `.local/maps/dual-stationary-packet-results-20261026.json`.
COM17 reception was then stopped successfully for the next physical dongle
checkpoint (`release-dual-com17-swap-stop-20261026.log`). COM39 remains receiving.

## Still open

- Resolve or explicitly disposition missed stress texts and the C2 attempt;
  neither is silently counted as a passing required protocol/load row.
- Five unplug/replug recovery cycles per dongle and physical screen/audio checks.
- Blog V4 and Nooelec V5 controlled checks; both are available from the owner.
- Remaining decoder-by-dongle protocol and HF qualification; nothing here proves
  FM/RDS, ADS-B, FT8, P25, ACARS, POCSAG or SSTV acceptance.
- Final common release artifact, merge, tagged-package validation and publication.

No overall release approval follows from this partial hardware record.
