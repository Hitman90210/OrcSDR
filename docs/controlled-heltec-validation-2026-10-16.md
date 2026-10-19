# Controlled Heltec validation — COM24 to COM39

Controlled live NODEINFO, position, missing-field retention, SD logging and reboot
persistence checks passed. The user also confirmed seeing the test messages on
the Tab5. No application source changes, builds or flashes were needed for this
follow-up; COM39 retained the preceding NODEINFO cache fix.

## Setup

- Transmitter: user-identified COM24, confirmed Meshtastic Heltec V4, node
  `!aa745cf5`, short name `hrdc`, long name `Hardcoreerik`.
- US, LongFast, automatic/default channel, no frequency override, transmit enabled,
  5 dBm. Test packets used hop limit zero with no response request or ACK retries.
- Receiver: COM39 Tab5, V3c, LoRa antenna, US LongFast slot 20, 906.875 MHz.
- Firmware SHA-256:
  `9ceb2c01bf56acce52cceb6546b331d3476c7e227e066a0e2cc75c82a5a20e41`.
- The Heltec had no GPS/saved position. The user authorized arbitrary test
  coordinates. All test coordinates are synthetic, not the user's location.

## Results

- NODEINFO packet `ebe47c2e`, port 4: decoded and saved as `hrdc / Hardcoreerik`.
- Text packet `c049202f`, port 1: decoded and saved; cached name was retained.
- Controlled position packets, port 3, were decoded and saved after temporarily
  enabling channel position sharing. Successful packet IDs include `023f24d9`
  and `49e0c8da`.
- Later text packet `8b8746b0`, port 1: decoded and saved; both the name and last
  position were retained.
- Transmitter-assigned packet IDs matched the receiver's CSV records with
  `from_host=0`. These were real RF receptions, not serial-injected device data.
- The CSV was downloaded using hash-verified SD transfer. SHA-256:
  `80b1e92b9d9b7ece3f8efb0f85e561e2fdb7b9567d796813d391fe29c42577f9`.
- After reboot: 85 records replayed, zero rejected, 26 devices restored. The
  Heltec retained its name, position, UTC observations and 11 counted packets.
- Final logging status: requested=true, ready=true, error=false, queued=0,
  dropped=0. COM39 was returned to the LoRa screen.

## Position setting and test-harness corrections

The Heltec's original primary-channel `position_precision` was 0, disabling
position sharing. Initial probes at synthetic (0.001, 0.001) produced no usable
position. Precision was temporarily set to 15 for the controlled test, then
restored to 0 and verified through a fresh API connection before completion.
No fixed-position configuration, radio frequency, transmit power, owner name or
keys were changed. No private keys were logged or exported.

The enabled probe requested integer coordinates `(16777216, 16777216)`, or
`(1.6777216, 1.6777216)` degrees. Public-channel position precision rounded these
to `(16842752, 16842752)`, or `(1.6842752, 1.6842752)` degrees on air. The first
enabled harness expected the unrounded values and therefore failed despite
successful reception. The final check used the actual rounded values, matched
their RF packet IDs against SD, and verified their retention after reboot.
The synthetic position remains in the Tab5's test history; it is not a real
location observation.

Meshtastic documents position sharing disabled at precision 0 and reduced
precision on public channels:
[Channel configuration](https://meshtastic.org/docs/configuration/radio/channels/#position-precision).

These are successful controlled-packet checks, not a loss-free RF claim. One
enabled probe was not recovered, while the following probes succeeded. The
captured decoder reports showed capture drops=0; logging queue drops remained 0.

## Commands and evidence

Run from `F:/AI/OrcSDR-TEMP/tab5-lora-history`:

```powershell
python .local/lora-nodeinfo/heltec_live.py
# PASS, exit 0: HELTEC_NODEINFO_AND_MISSING_NAME_RETENTION_PASS.

python .local/lora-nodeinfo/heltec_position.py
# Exit 1: no usable controlled position while sharing was disabled.

python .local/lora-nodeinfo/heltec_position_enabled.py
# Exit 1: harness expected unrounded coordinates; usable positions were received.
# Finally restored precision=0 and verified it.

python .local/lora-nodeinfo/heltec_finish.py
# PASS, exit 0: RF packet IDs match SD, rounded position matches,
# name/position retention and reboot persistence pass.
# CONTROLLED_HELTEC_TEST_PASS.
```

Evidence is preserved under `.local/lora-nodeinfo/`: `heltec-live.log`,
`heltec-position.log`, `heltec-position-enabled.log`, `heltec-final.log`,
`heltec-tx.json`, `heltec-position-setting.json`, `heltec-readback.csv`, and the
scripts. Earlier failed harness runs are preserved, not discarded.

The prior startup Home UI regression restoration limitation remains outside this
test. Physical Map placement was not independently checked. Nothing was pushed
or merged, and COM17/COM40 were not accessed.
