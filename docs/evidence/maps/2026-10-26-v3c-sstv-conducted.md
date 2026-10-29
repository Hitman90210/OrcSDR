# Blog V3c conducted SSTV release checks — October 26, 2026

This continues the [measured conducted bench](2026-10-26-v5-sstv-conducted.md)
on COM17 with app `fd8b6de6`, driver 0.9.3 and the same source references.
The operator confirmed Blog V3c connected through TX1 → 30 dB pad → the same
test cable. Pluto was muted for the swap. COM39 remained untouched.

## Identity and settings

- Tab5 COM17 `30:ED:A0:E2:E6:95`, P4 app SHA-256
  `3c700d175e9d5f35f507bbb3d8c836c870b2fe61f6623df8d6ccf3987350b5e1`.
- Observed profile 2 `blog_v3_r820t2`, provisional false; matched before TX.
- 145.500 MHz, NFM, SSTV IF 100 kHz, receiver 240 kS/s, manual gain 40.2 dB,
  RTL AGC off and bias off. This tests the VHF tuner path, not the V3c HF path.
- Source 2.4 MS/s, 200 kHz analog filter, 2.5 kHz deviation, 60 dB programmed
  attenuation, 0.5 drive. The unchanged chain previously measured −71.5 dBm
  with a carrier at 145.500 MHz; no new band or source power was selected.
- WAV/PNG identities and the unchanged guarded source harness are recorded in
  the V5 bench record. No receiver source, driver or C6 change was made.
- Source-off baseline produced no new SSTV image.

## Invalid source attempt and retry

The first Robot36 transmission aborted because Pluto's DMA underflow count
changed **(91, 0) → (92, 0)** during the waveform. The receiver retained a
5/240-row partial image, ended SYNC LOST, and correctly skipped saving it as
too few rows. This is an **invalid source attempt**, not a decoder failure or
pass. The required mode remains unqualified until a valid complete source run.
Fresh `orc_sstv stop` readback confirmed TX mute after the fault.

The retry uses the existing bench's HIGH process-priority scheduling approach,
with an explicit priority readback. The waveform generator, RF settings, DMA
fault rejection, identity/attenuation guards and shutdown code are unchanged.
A local wrapper initially failed its Windows API preflight before contacting
RF hardware; corrected 64-bit HANDLE bindings then enabled and verified HIGH.
That preflight failure is not an RF transmission or firmware failure.

The clean Robot36 retry completed with unchanged in-waveform DMA counters and
verified LO shutdown. Receiver serial/session 2 was VIS 8, 240/240 rows,
non-partial COMPLETE, 240 syncs locked, zero missed syncs, audio gaps and
overruns. It saved `rx_1793066509_145500000_2.bmp`, 230,454 bytes, in 87 ms
without an archive error. The cumulative partial count of one belongs to the
earlier invalid-source attempt, not the complete retry.

Because the wrapper preflight delayed TX, the initial receive recorder finished
before the retry's final image; a separate final query confirmed the complete
new image and its SD-index row. It must not be inferred from the earlier partial.

PD120 completed on a valid source run: VIS 95, 496/496 rows, non-partial COMPLETE,
248 syncs locked, zero missed syncs, audio gaps or overruns. Serial/session 3
saved `rx_1793066704_145500000_3.bmp`, 952,374 bytes, in 250 ms without error.

The first Scottie S1 source run also aborted on a DMA fault, **(98, 0) → (99, 0)**,
despite verified HIGH priority. The receiver ended SYNC LOST at 149/256 rows;
its useful partial image was saved as `rx_1793066851_145500000_4.bmp`. This is
not a full-frame pass or decoder failure. Fresh source mute readback passed.

Read-only host investigation found the active source URI using the LAN route
through a 2.5 Gbps USB Ethernet adapter. A Pluto USB/RNDIS adapter also exists,
but `orc_sstv check --uri ip:192.168.2.1` failed; no direct-path RF was attempted.
Host networking was not reconfigured. CPU samples near 15% and zero NIC error/
discard counters were taken after the fault; they do not prove its cause or
rule out transient starvation. A read-only SSH attempt stopped at host-key
verification, without authentication or remote changes.

One further instrumented Scottie S1 retry was prepared on the unchanged measured
LAN/RF chain. The local wrapper now emits the existing `pluto.LAST_STATS` after
completion/failure, without modifying source guards or waveform generation.
The instrumented retry completed with unchanged DMA counters and verified LO
shutdown. Source timing: 1,106 blocks, median 100.0 ms, p99 102.4 ms, maximum
118.1 ms; no block stalls over 150 ms. Scottie S1 serial/session 5 completed
256/256 rows, VIS 60, non-partial COMPLETE, 256 syncs locked, zero missed syncs,
audio gaps and overruns. The archive saved `rx_1793067371_145500000_5.bmp`,
245,814 bytes, in 91 ms without error. The earlier partial remains excluded.

All three complete images passed serial readback and exact-source RGB scoring
with the existing correlation ≥0.86 / MAE ≤40 per-channel limits:

| Mode | Correlation R/G/B | MAE R/G/B |
|---|---|---|
| Robot36 | 0.94901 / 0.95244 / 0.94868 | 11.72 / 10.63 / 11.73 |
| PD120 | 0.88414 / 0.88169 / 0.88324 | 16.82 / 16.19 / 16.85 |
| Scottie S1 | 0.95803 / 0.95856 / 0.95744 | 9.69 / 8.36 / 9.73 |

Source PNG/WAV hashes were checked against their generation manifests before
scoring. The PD120 readback was visually inspected as a complete reference
pattern with edge waviness; a pass does not imply pixel-identical RF output.

| Readback | SHA-256 |
|---|---|
| Robot36 BMP | `ec3cf10051b21b9ea8a0d0a306924d4a7438f8282d63b755f265ed3b86b5af9b` |
| PD120 BMP | `20564a7142fa30932bfafd79e464e7c23e70753471d55c3f75c062388b6605b3` |
| Scottie S1 BMP | `2d56291a65ab0b107d2d4e708c49bde42b2897a7c2553ae38b4ed7d0b9b020c6` |
| SD index | `207829c8e687345dd1fd58e23693d74723bbc377496eef3c5096dfc5a2ce57ae` |

The ten-row SD index preserves the entire earlier V4 index prefix. Each new
complete V3c image has exactly one matching complete index row. The saved
source-interrupted Scottie partial is retained but not scored as complete.

Final readback: Home/IDLE, profile 2, gain 40.2 dB, bias/RTL AGC off, driver
drops/overruns zero, uptime 4,021,011 ms, reset reason 11. No new reset during
this block. Logger ready, queue/pending/dropped zero. Before reclaim, PSRAM free
was 1,612,152 bytes, DMA free 9,955 bytes, largest DMA 7,680 bytes.

The named `RTL_MEM RECLAIM` command then released 18,756,144 bytes from registered
LoRa IQ (15,840,000), map frame (1,799,952) and SSTV picture (1,116,192) holders.
Afterward PSRAM free was 20,595,904 bytes, largest PSRAM 15,728,640; all three
holder byte counts were zero. Internal free was 30,359 bytes; DMA free 10,071,
largest 7,680. This proves the explicit reclaim path, not automatic reclaim on
every navigation or universally healthy DMA margins. The command plan passed
and the receiver remained IDLE; no SD file deletion was requested.

The three V3c conducted SSTV mode/image/storage checks are PASS for this exact
configuration. Other decoder rows, HF, physical audio/control checks and the
earlier reset investigation remain open. Next requested hardware is V4L on COM17
through the same measured chain; COM39 firmware remains untouched.

## Evidence

Receiver logs beside invocation in the integration worktree:

- `release-gate-sstv-v3c-prepare-20261026.log`
- `release-gate-sstv-v3c-robot36-20261026.log` (invalid-source run)
- `release-gate-sstv-v3c-robot36-priority-retry-20261026.log`
- `release-gate-v3c-robot36-retry-final-20261026.log`
- `release-gate-sstv-v3c-pd120-20261026.log`
- `release-gate-sstv-v3c-scottie1-20261026.log` (invalid-source run)
- `release-gate-sstv-v3c-scottie1-priority-retry-20261026.log`
- `release-gate-v3c-sstv-readback-20261026.log`
- `release-gate-v3c-sstv-image-comparisons-20261026.log`
- `release-gate-v3c-post-sstv-reclaim-20261026.log`

Source logs beside invocation in `F:/Ai/PlutoSDR/sstv/`:

- `release-gate-v3c-robot36-tx-20261026.log` (DMA fault)
- `release-gate-v3c-robot36-priority-retry-tx-20261026.log` (preflight failure)
- `release-gate-v3c-robot36-priority-retry2-tx-20261026.log` (complete source)
- `release-gate-v3c-pd120-tx-20261026.log` (complete source)
- `release-gate-v3c-scottie1-tx-20261026.log` (DMA fault)
- `release-gate-v3c-scottie1-priority-retry-tx-20261026.log` (instrumented retry)

Raw evidence and local command plans stay outside Git. Physical screen/audio
acceptance for this V3c block has not yet been established.
