# V5 conducted SSTV release checks — October 26, 2026

This is a partial release-gate record. It does not qualify other dongles,
decoders, HF reception or the complete release.

## Identities and measured chain

- COM17 USB `30:ED:A0:E2:E6:95`; Nooelec NESDR SMArt V5, profile 3,
  driver 0.9.3. P4 source `fd8b6de6`, app SHA-256
  `3c700d175e9d5f35f507bbb3d8c836c870b2fe61f6623df8d6ccf3987350b5e1`.
- Pluto `ip:192.168.1.228`, serial `b0d85d89da56de55b2ee997b00499360`,
  FISH Ball Rev.A / AD9361, firmware v2.3. Source ownership was confirmed by
  the operator. No firmware update was performed.
- tinySA Ultra COM7, serial `400`, version `tinySA4_v1.4-151-g31a89ce`,
  hardware `V0.4.5.1.1`. COM30 was the earlier mistaken port and was not used.
- Owner-confirmed chain: TX1 → 30 dB pad → cable → tinySA input for measurement;
  the same cable was then moved to COM17's V5 after TX mute and receiver stop.
  The 915 MHz antenna was removed. COM39 was not accessed in this block.
- Source 145.500 MHz, 2.4 MS/s, 200 kHz analog filter, 60 dB programmed
  attenuation, 0.5 digital drive; SSTV NFM deviation 2.5 kHz.
- Carrier calibration, tinySA input mode, 3 kHz RBW, 101 points over ±30 kHz:
  both on scans **−71.5 dBm at 145.500 MHz**, off peaks −113.0/−111.5 dBm.
  This is a fresh carrier measurement at the receiver end of the cable, not a
  new sensitivity measurement or integrated power measurement of modulated RF.
- Calibration ended with TX LO powerdown `1` and both TX attenuations
  `−89.75 dB`. Each completed protocol transmission also verifies shutdown.

The first calibration stopped before TX because the local parser did not accept
the instrument's scientific notation and third column. The next attempt saw
only noise; the operator found and corrected a connection. Neither attempt
qualified the chain. The subsequent valid measurement is retained separately:
`release-gate-calibration-145500000-20261026-172809.{json,log}`.
tinySA command reference: https://tinysa.org/wiki/pmwiki.php?n=Main.USBInterface.

## Source provenance

The existing `F:/Ai/PlutoSDR/sstv/orc_sstv` harness supplies waveform generation,
identity checks, attenuation readback, DMA fault rejection and mute-before-cancel.
It is not a Git checkout; source file SHA-256 values identify this local version:

| File | SHA-256 |
|---|---|
| `pluto.py` | `f5c897a0a565c5998f007cc915b6b05cc8cee60c5e58e8916370f6d057ef87df` |
| `protocol.py` | `bc77993646dc32b4eface3055210393d48beecebe470a209d818dc8612a48b8f` |
| `modulation.py` | `d3968166b4374fd0785949be18d2d82718dbc17df90b73654f791aa76d4718b6` |

`python -m orc_sstv generate --all --color` generated exact PNG/WAV references
under `F:/Ai/PlutoSDR/sstv/output/20261026-172916-290660/` and passed offline
VIS/duration/image checks. Each WAV also decoded completely through the compiled
production OrcSDR host receiver (`sstv_wav_replay.cpp`). No source generator was
invented as a replacement for protocol acceptance.

| Mode | WAV SHA-256 | Host replay |
|---|---|---|
| Robot36 | `5991bcc9af809bf2b7c75c1e1e9959df7b6a9e69ef4c72be0d61c58f3be7c043` | VIS 8, 240 rows, no missed syncs, complete |
| PD120 | `a78da101d98eae538dc2e073d65434437d579c4daa9f9e4f96090537c2dbb213` | VIS 95, 496 rows, no missed syncs, complete |
| Scottie S1 | `896b55e0e83ea934195eac29a48fffe9ecf5217d1b74ad0f1afef4c3129f1ecd` | VIS 60, 256 rows, no missed syncs, complete |

## Acquisition investigation and first accepted frame

At 20.7 dB manual tuner gain, two valid-source Robot36 attempts produced no VIS
or frame. The second attempt's actual 12 kHz decoder-input WAV was saved and
read back; host replay also produced no frame. Its SHA-256 is
`00d8df5ddb48d3b9a5e66e2aa3dc1dabf8fd03bc5d8a799abfa8fb8243038fa3`.
These failed reception attempts are not relabeled passes. No source DMA fault
occurred during either waveform; absolute startup counters are not zero but
remain unchanged during each transmission.

A known-tone diagnostic recorded a strong low-frequency component and displaced
tone peaks at 20.7 dB gain. Changing only receiver gain to 40.2 dB produced live
estimates near 1902, 1201 and 2300 Hz for the corresponding exact source tones.
This supports gain-dependent acquisition; it does not establish a root cause in
the driver, demodulator, source or analogue hardware. No receiver source code or
decoder algorithm was changed to obtain a pass.

At 40.2 dB gain, 145.500 MHz NFM, 100 kHz SSTV IF width, 240 kS/s receiver rate,
RTL AGC off and bias off, Robot36 completed **240/240 rows, VIS 8, no missed
syncs, gaps or overruns, non-partial, COMPLETE**. The SD worker saved
`rx_1793061784_145500000_1.bmp`, 230,454 bytes, without a write retry/error.
The serial file readback passed its SHA-256 transport check.

Direct comparison with the exact source PNG passes the existing host colour
limits (every channel correlation ≥0.86 and MAE ≤40):

| Channel | Correlation | Mean absolute error |
|---|---:|---:|
| R | 0.95088 | 13.72 |
| G | 0.95439 | 13.04 |
| B | 0.95055 | 13.73 |

The saved BMP was visually inspected as a complete reference pattern. Physical
screen appearance and audible quality remain separate operator checkpoints.
The source-off baseline produced no SSTV frame.

## Logs and current limits

Raw logs remain beside invocation in the OrcSDR integration worktree:
`release-gate-sstv-v5-prepare-20261026.log`,
`release-gate-sstv-v5-robot36-20261026.log`,
`release-gate-sstv-v5-robot36-diagnostic-20261026.log`,
`release-gate-sstv-v5-robot36-gain402-20261026.log`,
`release-gate-v5-robot36-gain402-readback-20261026.log`.
Reference replay logs are `release-gate-sstv-{robot36,pd120,scottie1}-reference-20261026.log`.
Source TX logs remain in `F:/Ai/PlutoSDR/sstv/`, named
`release-gate-v5-*-tx-20261026.log`; waveform metadata and hashes remain in the
source output directory. Binary evidence stays outside Git under `.local/maps/`.

The first audio readback was refused as `radio_busy`: SSTV restarted streaming
while its dashboard remained open. Leaving SSTV for Home before stopping the
radio allowed the readback. This is recorded as harness sequencing, not an SD
data failure. A local PD120 plan-generation quoting error occurred before any
device command/TX; the corrected plan is a separate invocation.

PD120 also completed 496/496 rows, VIS 95, no missed syncs, gaps or overruns,
non-partial COMPLETE. The SD archive saved `rx_1793062047_145500000_2.bmp`,
952,374 bytes, in 237 ms without a write error. Readback SHA-256:
`309043257d57b4455fa13b7971948148f836446fc1c0838a85c3117ba363c397`.
Direct reference comparison passed the same existing limits: correlation
R/G/B 0.87747/0.87491/0.87671, MAE 19.89/19.69/19.91. Visual inspection showed
a complete pattern with more edge waviness than Robot36; a pass is not a claim
of pixel-identical RF reception. Logs: `release-gate-sstv-v5-pd120-20261026.log`
and `release-gate-v5-pd120-readback-20261026.log`.

Robot36 BMP readback SHA-256:
`2375eb4a23e4235d305c06d4b916209e28607ad4ad2f837a0ab7c6ffa300e75b`.

Scottie S1 completed 256/256 rows, VIS 60, no missed syncs, gaps or overruns,
non-partial COMPLETE. The SD archive saved `rx_1793062263_145500000_3.bmp`,
245,814 bytes, in 82 ms without a write error. Readback SHA-256:
`37d76eb7f026ea671083076e206df66b1142d940c73477c3e2b8cb2b3a77c8f2`.
Direct reference comparison passed: correlation R/G/B
0.95604/0.95657/0.95511, MAE 11.79/11.31/11.85. The complete reference pattern
was visually inspected. Logs: `release-gate-sstv-v5-scottie1-20261026.log` and
`release-gate-v5-sstv-final-readback-20261026.log`.

The SD index was read back and contains all three complete receptions, matching
the corresponding mode, dimensions, row count and saved filename. Its SHA-256
is `e403072f700841bae19fe61fe6614e9a63afb92054fe380761498eccae43f15d`.
Final COM17 state: Home, receiver IDLE, manual gain 40.2 dB, bias off. Logger
ready, queued/pending/dropped zero. Uptime 10,093,324 ms, reset reason 11;
no unexpected reset during this block. DMA free 7,467 bytes, largest 6,400;
PSRAM free 2,968,592 bytes. These low internal-memory readings remain recorded,
not claimed as a general memory-health pass. C6, NVS and existing data were not
erased or replaced.

`bash tools/sstv_host_tests.sh` completed with overall PASS, including supported
colour modes, archive, capture, memory/reclaim and pattern checks. Its optional
real-WAV step was SKIPPED because no environment fixture was supplied; the three
explicit production-receiver reference replays above ran separately and passed.
Log: `release-gate-sstv-all-modes-host-20261026.log`; individual logs and
`sstv-summary-20261026-175152.log` are beside invocation. The script's Git
identity fields were blank under WSL; Windows Git confirmed no UI/main source
diff from tested `fd8b6de6`, with branch head `44561fb3` at invocation.

The V5's three conducted SSTV mode/image/storage checks are PASS for this exact
configuration. The owner initially was not present, then reported: "I saw the
test images now. looked good." This accepts observed displayed-image appearance;
it does not establish control responsiveness during reception or audible quality.
Those physical checks remain open. The next requested swap is Blog V4 on COM17 using the same
attenuated cable, with TX muted. Other dongles are not qualified by these results.
The full release gate remains blocked until all required combinations and
remaining investigations are addressed.
