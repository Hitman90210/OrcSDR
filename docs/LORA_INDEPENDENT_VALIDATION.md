# Independent LoRa / Meshtastic validation

Status: first-milestone hardware baseline complete; trigger/DSP code unchanged.

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
| Reference RX | COM16 | LILYGO T-Beam S3 Core | `2.7.26.54e0d8d` | Region enum 1 (US), modem preset enum 0 (LongFast), automatic channel. |
| Experimental RX | COM17 | M5Stack Tab5 / OrcSDR | current upstream build | US LongFast slot 20, 906.875 MHz, automatic capture enabled. |
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

The later inventories record the temporary USB disappearance rather than hiding
it. After reconnection, `artifacts/lora_validation/20260912-173152/` independently
identified all three roles and saved their non-secret configuration snapshots.

Four receive-only windows and three controlled LongFast runs are now recorded:

| Run | Duration / TX | Reference RX | OrcSDR RF | Preamble / header | CRC / Meshtastic | Zero-preamble false triggers | Drops |
|---|---:|---:|---:|---:|---:|---:|---:|
| `20260912-173415` | 900.0 s | n/a | 11 captures | 11 / 11 | 4 / 4 | 0 | 0 |
| `20260912-174945` quiet phase | 180.0 s | n/a | 1 capture | 1 / 1 | 1 / 1 | 0 | 0 |
| `20260912-174945` numbered TX phase | 10 TX | 8/10 | 8/10 windows | 7/10 / 7/10 | 3/10 / 3/10 | n/a | 1 |
| `20260912-175758` quiet phase | 180.0 s | n/a | 5 captures | 5 / 5 | 2 / 2 | 0 | 0 |
| `20260912-175758` numbered TX phase | 10 TX | 9/10 | 9/10 windows | 9/10 / 9/10 | 4/10 / 4/10 | n/a | 0 |
| `20260912-180909` quiet phase | 180.0 s | n/a | 0 captures | 0 / 0 | 0 / 0 | 0 | 0 |
| `20260912-180909` numbered TX phase | 10 TX | 10/10 | 10/10 windows | 5/10 / 5/10 | 1/10 / 1/10 | n/a | 1 |

Across 1,440 seconds of receive-only observation, OrcSDR started 17 captures.
Every completed candidate contained a detected LoRa preamble, so the fork's
reported 24 zero-preamble triggers in 180 seconds were **not reproduced** in
this RF environment. Seven quiet-window candidates reached CRC-valid encrypted
Meshtastic packets, showing that uncontrolled LoRa traffic was present rather
than a truly silent RF channel.

The first two controlled runs used an MLA-30+ outdoors approximately 50 feet
from the LoRa devices. The comparison run used a 915 MHz whip indoors
approximately 10 feet from them. Cabling was shielded as stated by the operator.
Because antenna, distance, and placement changed together, these are whole
receive-setup results rather than a controlled antenna-only or calibrated-power
comparison.

With the MLA-30+ setup, COM16 received 8/10 and 9/10 while OrcSDR produced 3/10
and 4/10 CRC-valid Meshtastic packets. With the closer indoor whip, COM16
received 10/10 and OrcSDR triggered in all ten TX windows, but only five windows
reached a preamble and one reached a CRC-valid Meshtastic packet. The whip run
also recorded one `capture_buffer_waiting` drop. Its capture-to-decode average
was 7.986 seconds and p95 was 12.070 seconds, compared with 5.554 seconds and
6.038 seconds in the immediately preceding MLA-30+ run.

The OrcSDR wide-window readings also changed materially: mean TX-window
`signal_dbfs`/`noise_dbfs` were approximately -4.72/-29.40 for the preceding
MLA-30+ run and -20.08/-15.73 for the whip run. These are receiver diagnostics,
not calibrated RF power measurements. The observed result does not support a
simple closer-is-better conclusion and should be repeated before assigning a
cause.

### Randomized TX/control-slot comparison

To account for uncontrolled LoRa traffic, the next paired experiment used the
same deterministic randomized schedule for both receive setups: ten TX slots,
ten no-TX control slots, 20 seconds per slot, and seed `90210`. Meshtastic and
OrcSDR configuration snapshots match across the pair.

| Receive setup | Reference RX | TX RF | TX preamble | TX CRC | Control RF / preamble / CRC | Zero-preamble TX attempts | Drops | Average / p95 decode |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| 915 MHz whip, indoor, ~10 ft | 10/10 | 10/10 | 2/10 | 1/10 | 0/10 / 0/10 / 0/10 | 9 | 0 | 7.609 / 15.665 s |
| MLA-30+, outdoor, ~50 ft | 10/10 | 10/10 | 10/10 | 5/10 | 3/10 / 3/10 / 0/10 | 2 | 2 | 5.613 / 13.334 s |

The whip's no-TX controls were empty, while all ten TX slots triggered OrcSDR.
This makes unrelated traffic an unlikely explanation for its nine
zero-preamble attempts in this run. The MLA-30+ observed RF/preambles in three
control slots, proving that its TX-window activity can be contaminated by
uncontrolled traffic; none of those control slots reached CRC.

The paired result favors the MLA-30+ setup for preamble and CRC yield, but it
does not isolate antenna performance: antenna type, placement, distance, and
test time differ. OrcSDR still lacks payload identity, so randomized controls
support statistical comparison rather than exact attribution of every
CRC-valid packet. A second counterbalanced pair is required before assigning a
physical cause.

OrcSDR does not yet emit decoded payload identity. Association to a numbered
TX therefore remains a non-overlapping host-time-window estimate. In the first
controlled run, two extra decode attempts occurred inside TX windows, but they
are not claimed as duplicate packet decodes. Its corrected
capture-to-completed-decode time averaged 6.197 seconds with an 8.466-second
p95 using matching OrcSDR capture sequence IDs.

Raw local evidence:

- `artifacts/lora_validation/20260912-173415/`
- `artifacts/lora_validation/20260912-174945/`
- `artifacts/lora_validation/20260912-175758/`
- `artifacts/lora_validation/20260912-180909/`
- `artifacts/lora_validation/20260912-182146/`
- `artifacts/lora_validation/20260912-183330/`

### IQ retrieval stability finding

After the paired runs, a manual four-second PSRAM IQ capture at 960 ksample/s
completed, but host retrieval ended early with a serial timeout. The subsequent
abort command also timed out, and the operator observed the Tab5 blue-screen
and reboot to Home at 906.875 MHz. No IQ file was retained.

The boot-time reset reason was not captured, so this is recorded only as an
unclassified reboot during sustained IQ retrieval. It is not attributed to a
watchdog, brownout, USB fault, or another cause. The same retrieval path should
not be repeated with multiple outstanding chunk requests.

The host transfer was subsequently changed to keep exactly one 2 KiB chunk
request outstanding and to tolerate transient empty serial reads within a
bounded deadline. No firmware or SD-card write was required. Two matched manual
captures then transferred directly from PSRAM to the PC without retry or reboot:

| Receive setup | Raw IQ | Transfer | SHA-256 | Host decode |
|---|---:|---:|---|---|
| MLA-30+, outdoor, ~50 ft | 7,680,000 bytes | 46.752 s | `7c6f476e9a8b6740f69b44ba985a19626f88ab1a7fad0ddb03dae2ed134767b7` | `ORC-IQ-MLA30-20260912-192454` |
| 915 MHz whip, indoor, ~10 ft | 7,680,000 bytes | 46.535 s | `66028e4a1e83f4337b025480f0f3d890823572dbe0e0cca2ff86de64cd2ce9d3` | `ORC-IQ-WHIP915-20260912-192815` |

The Tab5 native decoder completed the MLA-30+ capture in 12.445 seconds and the
whip capture in 72.252 seconds. The latter found two preambles and one CRC
failure; offline host decoding still recovered the labeled CRC-valid packet.
These timings are capture-specific performance evidence, not an antenna-only
comparison.

No firmware was built or flashed. No trigger or DSP production code changed.
The next milestone can benchmark independently designed early-candidate
detector instrumentation against this measured upstream baseline without
depending on multi-megabyte IQ retrieval.
