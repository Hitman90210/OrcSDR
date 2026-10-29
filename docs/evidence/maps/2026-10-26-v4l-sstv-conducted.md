# Blog V4L conducted SSTV release checks — October 26, 2026

The operator moved Blog V4L from COM39 to COM17 and confirmed the same TX1 →
30 dB pad → test cable chain. COM39's tablet firmware and C6 were untouched.
This completes the four-dongle **three-mode SSTV image/storage** block, not the
whole decoder matrix, physical audio/control acceptance or release approval.

## Identity and configuration

- COM17 USB `30:ED:A0:E2:E6:95`; profile 4 `blog_v4l_r828s`, provisional false,
  driver 0.9.3, verified before RF operation.
- P4 source `fd8b6de6`, app SHA-256
  `3c700d175e9d5f35f507bbb3d8c836c870b2fe61f6623df8d6ccf3987350b5e1`.
- 145.500 MHz, receiver 240 kS/s, NFM, 100 kHz SSTV IF, manual gain 40.2 dB,
  bias off, RTL AGC off.
- Same source, WAV/PNG hashes, RF settings and carrier calibration as the
  [V5 bench record](2026-10-26-v5-sstv-conducted.md): 2.4 MS/s TX, 200 kHz
  analog filter, 2.5 kHz deviation, 60 dB programmed attenuation, drive 0.5;
  unchanged cable chain measured −71.5 dBm at 145.500 MHz with a carrier.
- The unchanged guarded source ran with verified HIGH process priority, as in
  the successful [V3c retry](2026-10-26-v3c-sstv-conducted.md). DMA counters,
  attenuation readback, source identity and mute-before-cancel remained enabled.
- Source-off baseline created no new SSTV image. The gallery was empty after
  explicit reclaim, while earlier saved SD images remained available.

## Results

| Mode | New session | Rows | Locked/missed sync | Image write | Result |
|---|---:|---|---|---|---|
| Robot36 / VIS 8 | 6 | 240/240 | 240/0 | 230,454 bytes, 83 ms | PASS |
| PD120 / VIS 95 | 7 | 496/496 | 248/0 | 952,374 bytes, 261 ms | PASS |
| Scottie S1 / VIS 60 | 8 | 256/256 | 256/0 | 245,814 bytes, 94 ms | PASS |

All three frames were non-partial COMPLETE, with no new audio gaps or receiver
overruns. All three source transmissions completed, with unchanged in-waveform
DMA counters and verified LO shutdown. Absolute startup source counters are not
zero. No source-fault retry was required in this V4L block. Recorded source
maximum block times were 111.8, 113.4 and 119.7 ms respectively, without stalls
over 150 ms.

Readback and direct comparison against each exact source PNG passed the existing
host limits: correlation ≥0.86 and MAE ≤40 in every RGB channel. The source
WAV/PNG hashes were reverified before scoring.

| Mode | Correlation R/G/B | MAE R/G/B |
|---|---|---|
| Robot36 | 0.94972 / 0.95295 / 0.94941 | 11.47 / 10.39 / 11.47 |
| PD120 | 0.88473 / 0.88208 / 0.88379 | 16.64 / 16.01 / 16.67 |
| Scottie S1 | 0.95989 / 0.96040 / 0.95927 | 9.42 / 7.96 / 9.47 |

The PD120 readback was visually inspected as a complete reference pattern with
some edge waviness. These are accepted reference-quality checks, not claims of
pixel-identical RF reception or independently observed physical screen/audio.

| Saved file | SHA-256 |
|---|---|
| `rx_1793068925_145500000_6.bmp` | `85d3a81fadf6b26316fff34d3869edeed03a5fa0604d85b2b173d5af719f7c83` |
| `rx_1793069090_145500000_7.bmp` | `6720156c6b922ee16e709fe16522fdff8b2548dc2c6f6abccf8d5a15ab3ae372` |
| `rx_1793069251_145500000_8.bmp` | `6081c6d46c389b2f531a50a3c59cf16969cb824ef1bba488c7dc7dd3d8ff449b` |
| SD index | `b6c706dd6a0720c710a039007bf19ec9d2d12ee5707aec1e46a6d1d3a479c12b` |

The 13-row SD index preserves the full previous V3c index prefix. Each new V4L
image has exactly one corresponding complete row. The earlier invalid-source
partial remains in the index and is not counted among the twelve accepted
complete images. All file readbacks passed their serial transport hashes.

## Final state and evidence

COM17 is Home/IDLE, profile 4, manual 40.2 dB, bias/RTL AGC off, driver
drops/overruns zero. Uptime 5,877,994 ms, reset reason 11; no new reset during
this block. Logger ready with queued/pending/dropped zero. PSRAM free
19,633,320 bytes, largest 15,728,640; internal free 30,327; DMA free 9,907,
largest 7,680. The previously reclaimed large buffers were not required by
this test; low DMA margins remain observations, not a general memory gate pass.
Pluto is muted after the final successful transmission. No app build/flash,
NVS erase, SD deletion or C6 update occurred.

Receiver logs beside invocation in the integration worktree:
`release-gate-sstv-v4l-prepare-20261026.log`,
`release-gate-sstv-v4l-{robot36,pd120,scottie1}-20261026.log`,
`release-gate-v4l-sstv-readback-20261026.log`,
`release-gate-v4l-sstv-image-comparisons-20261026.log`.
Source logs beside invocation in `F:/Ai/PlutoSDR/sstv/`:
`release-gate-v4l-{robot36,pd120,scottie1}-tx-20261026.log`.
Local images, scoring results and index checks stay outside Git in `.local/maps/`.

Next milestone: continue the remaining decoder-by-dongle rows with independently
validated sources and fresh per-band calibration; retain physical audio/control
checkpoints and investigate the earlier watchdog. No merge/tag decision follows
from completing SSTV alone.
