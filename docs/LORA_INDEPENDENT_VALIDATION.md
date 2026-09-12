# Independent LoRa / Meshtastic validation

Status: first-milestone harness implemented; RF baseline awaiting the receiver
and transmitter USB connections described below.

## Provenance

This work starts from OrcSDR `b6e7761f8cd6717ead133ed9a827c90f26a6f0b9`
on 2026-09-12. OrcSDR pins `esp-rtl-sdr`
`b175dfea6782faa97e512d4a2408767c75977527`, which is also the freshly fetched
head of that repository's `origin/master`. The driver repository has no
`origin/main`; `origin/master` is its advertised default branch.

The external investigation is prompted by measurements and experiments in
Hitman90210/OrcSDR. The fetched fork head is
`465039ecd87187c657757624f41c33e193540b9b`. No fork commit was cherry-picked
and no fork source was copied.

The principal external hypotheses to reproduce are:

| Fork commit | Reported observation; not yet independently reproduced |
|---|---|
| `81400e5343a6c914289b4bd311aea114fa5d7f58` | 24/24 false triggers in a 180-second sample, with 1.8-3.1 seconds of decode work per trigger and no preambles. |
| `72a12d8947191f4469c0276f02c9e692c468acd5` | A channel-energy discriminator reportedly reduced false triggers from 24 to 3-6 per 180 seconds. |
| `1712532ed48778f17f40a50b0d407ba19f1c2221` | The existing self-test did not exercise the DSP path; a synthetic chirp/resampling matrix exposed a shared-scratch race. |
| `7c4fa1e4bbd8c3e02efc19d8788151dc42ec694d` | An internal grid of twice the LoRa bandwidth reportedly supports 125, 250, and 500 kHz without increasing the 960 kS/s capture rate. Hardware results in that commit are synthetic DSP results, not an RF payload decode. |
| `560032272ffad6c7af98ae5cb2b20729559639a6` | Long-interleaved headers can reportedly be distinguished, but complete LI payload/FEC/CRC support remained open. |

Current upstream source inspection confirms that `lora_iq_offer()` still makes
its initial capture decision from `rtl_signal_dbfs`, the wide receive-window
level. That makes the false-trigger hypothesis technically plausible; source
inspection is not a reproduction measurement.

## Initial device and software inventory

Read-only identity probes produced the following observations. No Meshtastic
configuration was changed and no test packet was transmitted.

| Role | Observed port | Identity | Firmware | Current safe config observation |
|---|---:|---|---|---|
| Controlled TX | COM24 | Heltec V4 | `2.8.0.47db0e3` | Region enum 1 (US), modem preset enum 0 (LongFast), automatic channel, no frequency override. |
| Reference RX | COM16 | LILYGO T-Beam S3 Core | `2.7.10.94d4bdf` | Region enum 1 (US), modem preset enum 0 (LongFast), automatic channel. |
| Experimental RX | not present | M5Stack Tab5 / OrcSDR | not observed | The historical COM17 assignment is not a device identity. |
| Unrelated USB serial | COM30 | USB Billboard device, VID `057E`, PID `0000` | unknown | It did not answer the read-only `RTL_STATUS` query and is not treated as OrcSDR. |

The PC has Meshtastic Python `2.7.11` and pyserial `3.5`. The Heltec firmware
release maps to the official Meshtastic firmware commit
[`47db0e3020a608e06fb65cce70cd2f093021bd82`](https://github.com/meshtastic/firmware/commit/47db0e3020a608e06fb65cce70cd2f093021bd82)
and release
[`v2.8.0.47db0e3`](https://github.com/meshtastic/firmware/releases/tag/v2.8.0.47db0e3).
The installed Python protobuf enumerates 14 presets, while this firmware source
also advertises newer profile-specific presets. The complete matrix must come
from the device's 2.8 region/preset map rather than the older client enum.

Two simultaneous Meshtastic devices materially improve the experiment: the
reference node can prove that the controlled text was actually received while
OrcSDR is evaluated independently. The baseline harness therefore requires all
three views.

## First-milestone harness

`tools/lora_lab/run_suite.py`:

- records USB inventory and confirms each explicitly assigned device identity;
- refuses to configure either Meshtastic radio;
- refuses OTA transmission without an explicit local-legality confirmation;
- verifies both radios already share US LongFast and the same primary channel,
  without retaining or printing the channel key;
- verifies the OrcSDR LoRa plan and automatic PSRAM capture state;
- records a quiet interval for false-trigger measurement;
- sends sequence-numbered `ORC-LORA-TEST-000001` messages;
- records only matching test payloads from the reference receiver;
- correlates OrcSDR capture/decode events within non-overlapping host-time
  windows; and
- saves raw logs plus JSON, CSV, configuration, inventory, and Markdown output
  under `artifacts/lora_validation/<timestamp>/`.

The current OrcSDR serial event does not expose decoded packet ID/text, so the
first revision's OrcSDR correlation is explicitly time-window based. Exact
payload-level correlation will require retained IQ decoded on the host or a
bounded machine-readable packet event. It is not claimed yet.

## Evidence and current gate

Host parser/correlation self-check: pass.

Local inventory evidence was saved under:

- `artifacts/lora_validation/20260912-163129/`
- `artifacts/lora_validation/20260912-163345/`
- `artifacts/lora_validation/20260912-163350/`

The later inventories record the USB disappearance rather than hiding it. The
Heltec COM24 port disappeared after repeated one-shot CLI queries, and COM16
subsequently became unavailable as well. The Tab5 PC-facing USB Serial/JTAG
interface was never present during this session. The required physical gate is
to reconnect the Heltec V4 and connect the Tab5 PC-facing USB cable. After port
rediscovery, the next run is a quiet-window baseline followed by controlled
legal US LongFast transmissions.

No firmware was built or flashed, no RF baseline was run, and no Hitman90210
performance claim has yet been reproduced. Trigger/DSP production code remains
unchanged until that baseline exists.
