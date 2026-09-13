# Independent LoRa / Meshtastic validation

Status: first-milestone hardware baseline complete; second-milestone offline
candidate-detector benchmark started; trigger/DSP firmware unchanged.

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

Matched no-controlled-TX captures were also collected. The MLA-30+ window had
no preamble in either native or host decoding. The whip window contained a
CRC-valid public-channel position packet from an uncontrolled node, so it is
labeled background LoRa rather than a negative vector.

### Initial staged-detector benchmark

`tools/lora_lab/candidate_detector.py` measures total-window power, the fraction
of FFT energy inside the configured LoRa channel, and repeated dechirped FFT
peaks. It uses only NumPy and the ORCIQ metadata; it does not call the full LoRa
decoder when computing these metrics.

| Capture | Power RMS p95 | In-channel ratio p95 | Consecutive chirps | Peak / median FFT |
|---|---:|---:|---:|---:|
| MLA-30+ controlled TX | 1.195940 | 0.987187 | 17 | 220.553 |
| Whip controlled TX | 1.196912 | 0.986759 | 16 | 215.044 |
| Whip no-controlled-TX, background LoRa present | 1.193266 | 0.982133 | 16 | 220.242 |
| MLA-30+ no-controlled-TX, no preamble | 0.048021 | 0.291400 | 2 | 3.368 |

This small corpus shows that power and channel occupancy identify RF activity,
while repeated chirp peaks distinguish the no-preamble capture from all three
LoRa-positive captures. It is not yet sufficient to choose a production
threshold or claim a false-positive rate. More negative interference classes
and weak-signal positives are required before firmware gating is enabled.

### Configured transmit-power pair

The Heltec was then configured for 15 dBm and 2 dBm for one controlled capture
on each receive setup. COM16 received all four exact tokens, and offline host
decoding recovered all four from their saved IQ. The Heltec was verified back
at 30 dBm and OrcSDR automatic capture was verified on afterward.

| Receive setup | Configured TX | Reference / host | Native result | Native time | Consecutive chirps |
|---|---:|---|---|---:|---:|
| MLA-30+, outdoor, ~50 ft | 15 dBm | exact / exact | preamble, CRC failure | 73.665 s | 17 |
| MLA-30+, outdoor, ~50 ft | 2 dBm | exact / exact | preamble, CRC failure | 73.738 s | 16 |
| 915 MHz whip, indoor, ~10 ft | 15 dBm | exact / exact | CRC valid | 12.485 s | 16 |
| 915 MHz whip, indoor, ~10 ft | 2 dBm | exact / exact | two CRC-valid packets in window | 12.638 s | 17 |

Power RMS p95 remained between 1.186538 and 1.196592 across these captures.
Thus the configured power change did not create a calibrated weak-signal series
at the SDR input in this setup. These files are useful positive vectors but
must not be presented as receiver-sensitivity evidence. Reproducible host-side
attenuation/noise impairment or a physically attenuated RF path is required for
that measurement.

### Deterministic offline impairment

The two original controlled captures were scaled from their measured power RMS
p95 to 0.05 RMS, then impaired with seeded complex Gaussian noise at 10, 5, 0,
-5, -10, -15, and -20 dB SNR. Seeds 90210-90216 were used for the MLA-30+
capture and 90217-90223 for the whip capture. One temporary ORCIQ file at a
time was quantized and passed to the full host decoder; no impaired IQ copies
were retained. Results are in the ignored lab artifact
`artifacts/lora_validation/corpus/impairment_benchmark.json`.

| SNR (dB) | MLA chirps / peak-to-median / host | Whip chirps / peak-to-median / host |
|---:|---|---|
| 10 | 17 / 115.257 / exact packet | 16 / 115.624 / exact packet |
| 5 | 17 / 73.430 / exact packet | 16 / 72.717 / exact packet |
| 0 | 17 / 43.907 / exact packet | 16 / 43.652 / exact packet |
| -5 | 17 / 26.168 / exact packet | 16 / 25.374 / exact packet |
| -10 | 17 / 14.291 / exact packet | 16 / 14.395 / exact packet |
| -15 | 17 / 8.874 / exact packet | 16 / 9.068 / exact packet |
| -20 | 17 / 5.705 / exact packet | 16 / 5.440 / exact packet |

Measured SNR was within 0.01 dB of every requested point. No clipping occurred
through -10 dB; -15 dB clipped at most 0.000156% of complex samples and -20 dB
clipped at most 0.951016%. Both controlled packet IDs remained recoverable at
every point, so this range does not establish a host-decoder failure boundary
or justify a production candidate threshold. These are deterministic synthetic
algorithm results, not calibrated RF sensitivity or antenna-performance data.

A coarse extension using the same 0.05 RMS target failed at -25 dB but clipped
about 21% of samples, so that apparent boundary was rejected. A refined scan
used the new `--target-rms 0.02` control. Five deterministic seeds per receive
setup were then run at each transition point (90500-90514 for MLA-30+ and
90515-90529 for the whip):

| SNR (dB) | MLA host decode | Whip host decode | MLA chirps | Whip chirps |
|---:|---:|---:|---:|---:|
| -21 | 5/5 | 5/5 | 13-17 | 16-17 |
| -23 | 3/5 | 4/5 | 5-16 | 9-16 |
| -25 | 1/5 | 0/5 | 3-9 | 3-10 |

Maximum clipping was 0% at -21 dB, 0.000234% at -23 dB, and 0.014844% at
-25 dB. This bounds the synthetic transition but still does not justify a
production threshold because the corpus does not yet cover non-LoRa
interference classes.

### Matched receive-only negative pair

One additional manual four-second receive-only capture was collected on each
antenna setup without a controlled transmission. Both host decodes reported no
LoRa preamble. Direct PSRAM-to-PC transfer completed without an SD write or
device reboot, and automatic capture remained enabled afterward.

| Receive setup | Power RMS p95 | In-channel ratio p95 | Consecutive chirps | Peak / median FFT | SHA-256 |
|---|---:|---:|---:|---:|---|
| MLA-30+, outdoor, ~50 ft | 0.048597 | 0.290769 | 2 | 3.368 | `38e1d39b93c2ab9e6e47b0bf6d3e395061268ff9b86e2b438424554cbda16a8f` |
| 915 MHz whip, indoor, ~10 ft | 0.149974 | 0.272990 | 2 | 3.457 | `072fbd6a17a2924e1630c965dd61df8b13aa5d3698891e82a2ee1c5d1b20771c` |

Together with the earlier MLA-30+ control, the corpus now has three confirmed
no-preamble captures across both receive setups. The higher whip power did not
produce repeated-chirp evidence. This remains too small to estimate a field
false-positive rate.

The first attempt to retain an automatic energy-triggered capture exposed a
current ownership boundary: firmware hands that buffer directly to native
decoding after `RTL_IQ_DONE`, so the host watcher received
`RTL_IQ_RETRIEVE_ERROR capture_not_ready`. Manual capture was used for the
matched pair. No automatic capture was represented as retained IQ.

### Corpus manifest and host/native differential baseline

`tools/lora_lab/corpus_manifest.json` assigns content-bound IDs to all ten
useful captures. It records SHA-256, ORCIQ metadata, ground truth, packet ID and
plaintext where controlled, reference/host/native results, setup, and evidence
provenance. Raw IQ remains only in the ignored local directory
`artifacts/lora_validation/corpus/`; it is not committed to Git. The verifier
checks every hash and header before host replay and writes its generated report
to `artifacts/lora_validation/corpus/differential_report.json`.

Current corpus composition is six controlled positives, one uncontrolled
background-LoRa positive, and three confirmed no-preamble negatives. Current
host replay recovered all six exact controlled packets and the unrelated
position packet. Native evidence is classified only when a durable sidecar or
specific engineering record supports it:

| Capture ID | Ground truth / packet ID | Host | Native | Class | Native stage | Host ms | Native ms |
|---|---|---|---|:---:|---|---:|---:|
| `orciq-7c6f476e9a8b6740` | controlled / `0xda4f1809` | pass | unknown | ? | unknown | 2999.573 | 12445 |
| `orciq-38e1d39b93c2ab9e` | no-preamble negative | fail | unknown | ? | unknown | 977.024 | unknown |
| `orciq-1d46413a2605eba5` | no-preamble negative | fail | fail | C | preamble search | 972.570 | unknown |
| `orciq-0f4812e88b88bd75` | controlled / `0xfb7a31ca` | pass | CRC fail | B | payload CRC | 2115.400 | 73665 |
| `orciq-f91ff779153ca577` | controlled / `0x6b7b15cc` | pass | CRC fail | B | payload CRC | 2058.937 | 73738 |
| `orciq-66028e4a1e83f433` | controlled / `0xa30f7cb9` | pass | unknown | ? | payload CRC evidence incomplete | 4352.008 | 72252 |
| `orciq-072fbd6a17a2924e` | no-preamble negative | fail | unknown | ? | unknown | 973.991 | unknown |
| `orciq-582e2fe308339967` | background LoRa / `0x55ebab82` | pass | unknown | ? | unknown | 2041.718 | unknown |
| `orciq-d9f355473ea17c15` | controlled / `0xeca065e3` | pass | pass | A | complete | 1989.679 | 12485 |
| `orciq-d3d7b68a288edd7a` | controlled / `0xc6c8e1e5` | pass | pass | A | complete | 4109.583 | 12638 |

Totals are A=2, B=2, C=1, D=0, and unknown=5. The two Class B MLA-30+
captures are the highest-priority permanent regression vectors: COM16 and the
host recovered the exact controlled payload, while native parsing passed the
explicit header and failed payload CRC after 73.665 and 73.738 seconds. The
first divergence is therefore currently bounded to payload symbol extraction,
clock/CFO correction, deinterleaving/FEC, or CRC input; the exact earlier stage
is not yet known. The 72.252-second whip capture is also pathological, but its
native packet outcome is not recorded precisely enough for A/B classification
and must be replayed rather than guessed.

### On-device deterministic replay and phase isolation

The Tab5 now has a laboratory-only replay path that accepts a content-bound
ORCIQ payload over the authenticated COM17 session, verifies its SHA-256, and
decodes it directly from PSRAM. Live RTL-SDR reception is stopped during the
upload and replay because both paths own the same IQ buffer. This proves native
decoder behavior for fixed input; it does not replace live capture-to-decode
acceptance, where the radio must remain active.

Replaying Class B capture `orciq-0f4812e88b88bd75` reproduced the historical
failure deterministically: one preamble, a valid explicit header, zero packets,
and one payload CRC failure. Three baseline runs completed in 61.683-61.898
seconds. Phase counters attribute about 43.3 seconds to 28 payload-symbol
hypotheses, about 9.9 seconds to preamble search, 5.6 seconds to filtering, and
2.3 seconds to resampling. This narrows the performance problem to repeated
payload demodulation rather than FEC, CRC, or Meshtastic parsing.

The host and native paths selected the same payload start sample (1,306,387)
with no native timing adjustment and produced 118 symbols. Their raw symbol
streams matched through zero-based index 13. The other 64 differences were
exactly one bin high on native; no difference exceeded one bin. Host CFO was
292.96875 Hz, while the first native pass reported 244.1 Hz. All traced native
FFT peaks lay exactly on its four-times FFT grid. These observations place the
first proven divergence in preprocessing or payload symbol estimation, before
FEC and CRC.

Two candidate explanations were tested and rejected. Changing fractional CFO
rounding did not change the CRC result, and parabolic refinement of the native
preamble peak produced a zero fractional offset and the identical 118-symbol
stream. Neither experiment remains as a decoder change. The next step is to
compare host preprocessing with the native filter and linear, quantized
resampler, then retain only the smallest change that makes the archived Class B
capture pass without regressing the Class A and negative corpus vectors.

During this phase firmware was built and flashed to COM17 for measurement. The
separate one-line PSRAM placement fix for the home spectrum buffer preserves the
tracked 40 KiB internal DMA reserve and restored boot with the default native
configuration. No production trigger threshold has been enabled, and no
decoder DSP correction has yet been accepted.
