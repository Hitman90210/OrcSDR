# POCSAG decoder validation — 2026-09-07

Validated on Tab5 COM17 with OrcPager CC1101 at 433.920 MHz, 1200 baud,
capcode 1234560, text TEST, configured power -30 dBm and three repeats per test.
The user confirmed the same transmissions decoded on Flipper Zero POCSAG_Pager.

## Findings and changes

- The original full-rate double-precision CIC arithmetic could not keep up:
  live telemetry reported 66,145 application IQ drops (USB drops/overruns zero).
  Replace unbounded integrator differences with bounded single-precision sums.
- Captured tones were approximately -9 and -18 kHz relative to the receiver LO.
  Discriminate at 192 kS/s before averaging to 38.4 kS/s, avoiding phase wrapping
  near the old discriminator limit. Track discriminator DC to center the slicer.
- Align integrate-and-dump symbol boundaries toward transitions with bounded
  one-sample adjustments.
- Extend the existing three-second IQ recorder to POCSAG for diagnosis, using
  the existing checksum-verified download path. Capture via RTL_IQ_START in
  POCSAG mode; retrieve using RTL_IQ_RETRIEVE_BEGIN / RTL_IQ_GET_BEGIN / chunks.

## Evidence

`./tools/test-pocsag-core.ps1` passed optimized and ASan/LSan/UBSan builds.
Synthetic CU8 tests decode TEST/1234560 with both polarities, arbitrary initial
phase, +/-6 kHz and +/-13.5 kHz offsets, and +/-1% symbol-clock differences.
Existing discriminator self-checks and silence checks also passed.

`./apps/orcsdr-tab5/tools/build-tab5-idf.ps1` passed. App-only flash to 0x10000
on COM17 reported `Hash of data verified`. Bootloader, partitions and NVS were
not flashed. Tested application SHA-256:

`5f901031b49186b217d5d4dde746c279a6d03e117c72d2e0f4adbf7532d2ce59`

After user transmissions, live status showed:

```text
baud=1200 inverted=1 batches=14 codewords=224 valid=224 corrected=0
uncorrectable=0 parity_failures=0 messages=14 truncated=0
usb_overruns=0 usb_drops=0 iq_drops=0
```

The user confirmed capcode 1234560 and TEST in the Tab5 message list.
Multiple tests were sent; this is not a controlled three-of-three delivery-rate
measurement. No live 512/2400-baud or weak-signal acceptance claim is made.

Local evidence is preserved under `artifacts/pocsag-validation/`, including
build/flash logs and the original 5,760,036-byte ORCIQ recording
`pager-20260907-183241.orciq` (SHA-256
`1b292d3bdeb46cb933ac91d833108e9f82665b3f27b29c8128846fd1df9dd796`).
That recording was made before the throughput fix and does not decode reliably;
it must not be represented as a clean regression fixture.

## Reference

Reviewed the user-provided [Flipper POCSAG_Pager implementation](https://github.com/xMasterX/all-the-plugins/tree/dev/base_pack/pocsag_pager)
and the local OrcPager RadioLib transmitter to compare polarity, symbol timing,
and framing. These changes independently implement the receiver DSP; no code
from that Flipper implementation was copied or redistributed.
