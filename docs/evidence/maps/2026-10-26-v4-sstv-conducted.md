# Blog V4 conducted SSTV release checks — October 26, 2026

This continues the [measured V5 bench procedure](2026-10-26-v5-sstv-conducted.md)
on the same COM17 Tab5, app and source fixtures. No firmware, driver, decoder or
source-harness changes were made between dongles. Other release-gate rows remain
open; a conducted SSTV result is not a complete four-dongle release approval.

## Identity and settings

The owner confirmed replacing the V5 with Blog V4 and reconnecting the same
TX1 → 30 dB pad → cable chain. The 145.500 MHz carrier measurement remains
−71.5 dBm at the end of this unchanged chain; no new band or source power setting
was introduced. TX was muted during the swap. COM39 remained untouched.

- COM17 `30:ED:A0:E2:E6:95`, source `fd8b6de6`; app SHA-256
  `3c700d175e9d5f35f507bbb3d8c836c870b2fe61f6623df8d6ccf3987350b5e1`.
- Driver 0.9.3, profile 1 `blog_v4_r828d`, provisional false.
- 145.500 MHz, receiver 240 kS/s, NFM, SSTV IF 100 kHz, manual gain 40.2 dB,
  RTL AGC off, bias off. Source settings and WAV/PNG hashes match the V5 record.
- Preflight uptime 10,228,485 ms, reset reason 11. Driver drops/overruns zero.
- Source-off baseline retained the earlier V5's last image (serial 3); it did
  not create a new reception. New V4 image serials must be greater than 3.

## Conducted results

Robot36 completed as serial/session 4: VIS 8, 240/240 rows, non-partial COMPLETE,
240 syncs locked, zero missed syncs, audio gaps and overruns. The archive saved
`rx_1793062583_145500000_4.bmp`, 230,454 bytes, in 83 ms without an error.
Final driver status remained profile 1/STREAMING with zero drops/overruns.
Pluto completed and verified LO shutdown; no in-waveform DMA counter change.

PD120 completed as serial/session 5: VIS 95, 496/496 rows, non-partial COMPLETE,
248 syncs locked, zero missed syncs, audio gaps and overruns. The archive saved
`rx_1793062768_145500000_5.bmp`, 952,374 bytes, in 240 ms without an error.

Scottie S1 completed as serial/session 6: VIS 60, 256/256 rows, non-partial
COMPLETE, 256 syncs locked, zero missed syncs, audio gaps and overruns. The
archive saved `rx_1793062940_145500000_6.bmp`, 245,814 bytes, in 88 ms without
an error. Each source transmission completed with unchanged in-waveform DMA
counters and verified LO shutdown. Absolute startup DMA counters are not zero.

All three images passed direct RGB comparison against the exact source PNGs,
using the existing host limits (correlation ≥0.86 and MAE ≤40 in every channel):

| Mode | Correlation R/G/B | MAE R/G/B | Result |
|---|---|---|---|
| Robot36 | 0.94779 / 0.95125 / 0.94756 | 11.76 / 10.65 / 11.76 | PASS |
| PD120 | 0.88449 / 0.88184 / 0.88357 | 16.69 / 16.04 / 16.72 | PASS |
| Scottie S1 | 0.95940 / 0.95996 / 0.95867 | 9.46 / 8.02 / 9.53 | PASS |

The PNG and WAV hashes were rechecked against the source-generation manifests
before scoring. These are new V4 images, not the cached V5 images. The PD120
readback was also visually inspected as the complete reference pattern, with
some edge waviness. No pixel-identical RF-image claim is made.

| Readback | SHA-256 |
|---|---|
| Robot36 BMP | `8833b478783b9552feec56bfcf7e0b90a7cd96bf409c13b892a8656b6be0d5cd` |
| PD120 BMP | `c56e2f87ac1e0f2b3472cbf5d9614bc77b91d1acfb5c2bfb9f8d22e3108c0c7e` |
| Scottie S1 BMP | `63d2c0f5bb27653567898678d8f97e819a8fdc3e4ae24b77404b6426ce7b6c44` |
| SD index | `d9680b977faacb69a0cbe6a704f3bf3bbb61f383385090b2e7091a41410506d4` |

The index contains the three new complete rows and preserves the earlier V5
rows. All serial readbacks passed their transport SHA-256 checks. Final state
is Home/IDLE, profile 1, manual 40.2 dB, bias off, driver drops/overruns zero,
logger ready and queued/pending/dropped zero. Uptime 10,790,561 ms, reset reason
11; no new reset during this block. PSRAM free 2,800,652 bytes; DMA free 7,463,
largest 6,400 bytes. These low DMA readings are preserved as observations, not
a general memory-health pass.

The three V4 conducted SSTV mode/image/storage checks are PASS at this exact
configuration. Required hotplug, other decoders, map-load, physical controls and
audio checks are not closed by this result. Five V4 recovery cycles were offered
as the next manual checkpoint; no unobserved cycle is counted.

Subsequent recovery/reset observations are recorded in the
[incident follow-up](2026-10-26-v4-hotplug-navigation-reset.md): three recoveries
were operator-reported, one controlled disconnect/navigation recovered without
a reset, and the original unintended reset remains unexplained. The five-cycle
gate is not marked passed. Post-restart readback confirmed the old index prefix
and this V4 Robot36 BMP were unchanged.

## Evidence

Receiver logs beside invocation in the integration worktree:
`release-gate-sstv-v4-prepare-20261026.log`,
`release-gate-sstv-v4-robot36-20261026.log`,
`release-gate-sstv-v4-pd120-20261026.log`,
`release-gate-sstv-v4-scottie1-20261026.log`,
`release-gate-v4-sstv-readback-20261026.log`,
`release-gate-v4-sstv-image-comparisons-20261026.log`.
Source logs beside invocation in `F:/Ai/PlutoSDR/sstv/`:
`release-gate-v4-{robot36,pd120,scottie1}-tx-20261026.log`.
Raw binary artifacts remain outside Git under `.local/maps/`.

The owner reported seeing the test images and that they looked good after the
V5 block. This is displayed-image appearance evidence; it does not qualify live
V4 control response or audio quality. Physical audio/control checks remain open.
