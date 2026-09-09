# Adding 500 kHz LoRa support (Long Turbo / Short Turbo)

Live worklog. Written to be handed off mid-flight: every decision, why it was
made, what is done, what is left, and how to verify each piece.

**Goal:** decode the 500 kHz Meshtastic presets *without* breaking the 250 kHz
LongFast path that already works.

| Preset | Bandwidth | SF | Coding rate | Status |
| --- | --- | --- | --- | --- |
| Long Fast (old US stock) | 250 kHz | 11 | 4/5 | works today, must not regress |
| **Long Turbo** (US replacement) | **500 kHz** | 11 | 4/8 | target |
| Short Turbo | 500 kHz | 7 | 4/5 | target |

The bench node is on **Long Turbo**, read off its own LoRa config screen as
"Long Range - Turbo". Worth recording because the two 500 kHz presets cannot be
told apart by frequency -- both live on the same slot grid -- and an earlier note
in this repo guessed Short Turbo from a half-remembered menu label. Read the
preset off the device, not the frequency.

Background: `FORK_HANDOFF.md` 3d (why it cannot decode today) and 5.7c (the DSP
regression vector this work is checked against).

---

## Status

| Stage | State |
| --- | --- |
| 0. Measure the constraints | **done** |
| 1. Thread a runtime decode rate | **done** |
| 2. Extend the DSP vector to 500 kHz | **done** |
| 3. Verify on hardware | **done** -- 10/10 cases pass |
| 4. Try a real 500 kHz node | not started |
| 5. Optional: a 500 kHz channel filter | **designed and verified, not shipped** |
| 6. Coding rate 4/8 | **done -- needed no new code** |

---

## Stage 0 — the two measurements that shaped the design

### The anti-alias IIR is a 250 kHz-channel filter and cannot be reused

`kLoraLowpass` in `lora_native_decoder.cpp`, five biquads, measured at the
960 kHz capture rate:

| Frequency | Response |
| --- | --- |
| 100 kHz | -0.03 dB |
| **125 kHz** (edge of a 250 kHz channel) | **-3.01 dB** |
| 150 kHz | -18.26 dB |
| 200 kHz | -49.60 dB |
| **250 kHz** (edge of a 500 kHz channel) | **-78.29 dB** |

Its -3 dB point is 125.0 kHz -- exactly half of 250 kHz, which is what it was
designed to be. Running a 500 kHz channel through it removes everything past
half the channel. **So the 500 kHz path must not use it.**

### A 960 kHz capture is already enough for a 500 kHz channel

This is the finding that removes the hard part.

```
960 kHz complex sampling covers  +/- 480 kHz
a 500 kHz LoRa channel spans     +/- 250 kHz
```

Complex (I/Q) sampling at 960 kHz represents 960 kHz of spectrum, not 480. The
channel fits with room to spare, so **nothing aliases** and no anti-alias filter
is needed on the 500 kHz path at all. The resampler runs *upward*, 960 kHz to
1 MHz, and upsampling has nothing to fold in.

**This corrects `FORK_HANDOFF.md` 3d item 2**, which said the capture rate had
to rise to supply two samples per chip at 1 MHz, and worried about doubling the
IQ buffer against a binding internal-RAM constraint. That was wrong. The "two
samples per chip" figure is a property of the decoder's *internal grid*, not a
Nyquist requirement on the capture: the decoder needs `2^(SF+1)` samples per
symbol so the FFT size is a power of two, and the resampler is what puts the
capture on that grid. Where those samples are interpolated from only has to
satisfy the sampling theorem, and 960 kHz does.

No driver change. No larger buffer. No internal-RAM cost.

### What sizes actually change

Nothing, which is the other pleasant surprise:

```
symbol_samples = 2^(SF+1)      chips, not seconds -- unchanged
fft_size       = 2^(SF+3)      unchanged
kMaxFft        = 32768         unchanged, so the 2 x 256 KB PSRAM scratch is unchanged
```

Only the *time base* changes: `t = i / decode_rate` instead of
`t = i / 500000`. The whole change is threading one number.

---

## Design

`decode_rate = 2 x bandwidth_hz`:

| Bandwidth | Decode rate | 960 kHz capture must | Anti-alias filter |
| --- | --- | --- | --- |
| 125 kHz | 250 kHz | decimate 3.84x | yes -- existing filter |
| 250 kHz | 500 kHz | decimate 1.92x | yes -- existing filter |
| **500 kHz** | **1 MHz** | **interpolate 1.042x** | **no -- nothing to fold in** |

The rule is simply whether the resampler is going down or up.

### The in-place hazard this creates

`decode_capture_pass()` currently does filter, then resample, and *both* write
into `g_scratch.resampled`. That is safe today only by an accident of
arithmetic: when decimating, the resampler's read index runs ahead of its write
index, so it never overwrites a sample it has yet to read.

When **upsampling that reverses** -- the read index falls behind the write index
and in-place resampling would corrupt its own input. The 500 kHz path avoids it
by skipping the filter, so the resampler reads the caller's capture buffer and
writes `g_scratch.resampled`: different buffers. This is load-bearing and easy
to undo by accident, so it is guarded explicitly in the code rather than left as
a comment.

---

## Stages 1-3 -- what changed, and the proof

### The change

`kDecodeRate` is gone. In its place:

```c
uint32_t decode_rate_for(uint32_t bandwidth_hz) { return bandwidth_hz * 2u; }

bool supported_bandwidth(uint32_t bandwidth_hz);   // 125k / 250k / 500k

// Decimating folds everything above half the decode rate into the channel and
// has to be filtered. Interpolating does not -- and the filter on hand would
// take the outer half of a 500 kHz channel with it.
bool needs_anti_alias(uint32_t bandwidth_hz, uint32_t sample_rate) {
  return decode_rate_for(bandwidth_hz) < sample_rate && bandwidth_hz <= 250000u;
}
```

Threaded through `configure_chirp()`, `interpolated_iq()`,
`linear_resample_capture()`, `dechirp_peak()` and `decode_capture_pass()`.

Three things worth knowing if you touch this again:

1. **`Scratch` caches the bandwidth, not just the SF.** `configure_chirp()`
   returns early when the cached key matches. Keyed on SF alone, a 500 kHz
   decode would be handed the 250 kHz chirp table -- whose sweep is half as
   steep -- and would present as a decoder that simply finds no preambles.
2. **`dechirp_peak()` reads the decode rate from `g_scratch`**, not from an
   argument, so it cannot disagree with the chirp table it is about to use.
3. **The in-place resample is only safe downward.** Guarded in
   `linear_resample_capture()`: it refuses when handed its own buffer while
   interpolating.

### Sizes are unchanged, as predicted

Binary grew 0x330 bytes. No new PSRAM, no new internal RAM, no driver change.

### Proof: the DSP vector, extended to 10 cases

Modelled on the host first, then run on hardware. Both agree:

| Case | Front end | Worst bin error (tolerance +/- 2) |
| --- | --- | --- |
| `sf7_250_raw` | - | 0 |
| `sf7_500_raw` | - | 0 |
| `sf7_250_front` | anti-alias, decimate 1.92x | 1 |
| `sf7_500_front` | none, interpolate 1.042x | **0** |
| `sf11_250_raw` | - | 0 |
| `sf11_500_raw` | - | 0 |
| `sf11_250_front` | anti-alias, decimate 1.92x | 1 |
| `sf11_500_front` | none, interpolate 1.042x | **0** |
| `sf7_125_raw` | - | 0 |
| `sf7_125_front` | anti-alias, decimate 3.84x | 0 |

The 500 kHz front end is *more* accurate than the 250 kHz one -- no IIR group
delay to cancel, and a 1.042x interpolation disturbs the grid far less than a
1.92x decimation. That was not the expected result and is worth remembering:
the path with no filter is the better-conditioned one.

On hardware:

```
RTL_LORA_NATIVE_SELF_CHECK_OK dsp_cases=boot elapsed_ms=432
RTL_LORA_SELFTEST ok failed_case=none elapsed_ms=1724
```

432 ms for the 5-case boot subset, against 366 ms for the previous 3 -- the two
added cases are SF 7, whose symbols are 256 samples against SF 11's 4096.

**This proves the signal path, not reception.** Synthesised symbols are clean,
perfectly aligned and noise-free. What is proved is that chirp generation,
resampling, dechirping and symbol mapping are correct at 500 kHz -- which is
exactly what was broken. Preamble detection, header parsing, FEC and CRC on a
real off-air burst are stage 4.

---

## Stage 6 -- coding rate 4/8 needed no new code

`FORK_HANDOFF.md` 3d item 4 said the decoder "has only been exercised on 4/5;
worth confirming that path". That overstates the risk. **The LoRa PHY header is
always sent at CR 4/8**, whatever coding rate the payload uses:

```c
for (size_t i = 0; i < codeword_count; ++i)
  nibbles[i] = hamming_decode(codewords[i], 8);   // parse_header(), always 8
```

So `hamming_decode()`'s correcting branch has run on every packet the decoder
has ever produced. The payload path calls the identical function with
`redundant_bits = coding_rate + 4`, which is the same 8 for CR 4/8.

What was genuinely untouched is narrower: `deinterleave()` over **8 rows**
instead of 5, and `symbol_count()` at `coding_rate = 4`. Both now have
assertions rather than an argument, in `fec_self_check()`:

| Assertion | Result |
| --- | --- |
| Hamming(8,4): every nibble has a zero-syndrome codeword | 16/16 |
| ... decodes clean | 16/16 |
| ... survives a flip in any of the 8 bit positions | **128/128** |
| CR 4/5 is parity-only and passes the nibble through untouched | 256/256 |
| `deinterleave()` round trip, SF 7 and 11, 5 and 8 rows | 4/4 |
| `symbol_count()` at CR 4/5 and 4/8 | 38 and 56 symbols |

The CR 4/5 assertion is the one worth keeping: 4/5 and 4/6 are parity-only,
they detect and never correct. A corrector firing there would silently rewrite
good data, which is worse than the error it thought it was fixing.

The round-trip test needs an interleaver, which the decoder does not have, so
`interleave()` is written in the module as the exact inverse of
`deinterleave()`: `codewords[column]` bit `row` is
`rotate_left(symbols[row], row % ppm, ppm)` bit `column`.

---

## Stage 5 -- a channel filter for the 500 kHz path (designed, not shipped)

### Why it is not just a noise-floor question

`dechirp_peak()` folds the spectrum in half:

```c
magnitude = |X[bin]| + |X[bin + bin_count]|
```

At a 1 MHz decode rate that pairs frequencies **500 kHz apart**, so a component
at +400 kHz -- well outside a +/- 250 kHz channel -- lands on the bin for
-100 kHz, which is inside it. Out-of-channel energy is not merely background;
the fold puts it directly onto the peak search. On 915 MHz ISM that is not
academic -- 3c exists because of interferers.

Measured over the 460 kHz that folds inward against a 500 kHz channel:

| | Noise raised by folding |
| --- | --- |
| Unfiltered | +2.83 dB |
| Filtered | +0.09 dB |
| **Recovered** | **2.74 dB** |

Plus adjacent-channel rejection: an interferer 300 kHz out goes from 0 dB to
-27.8 dB.

### What kLoraLowpass actually is

Reproduced exactly from a design script -- worst relative coefficient
difference **4.5e-09**:

> 10th-order Butterworth, fc = 125 kHz at fs = 960 kHz, five biquads ordered by
> **ascending pole radius**, overall gain folded into section 0's numerator,
> `{b0, b1, b2, a1, a2}` in transposed direct form II.

Worth writing down: it means new coefficients can be generated for this code
with confidence that they match its conventions, rather than guessed at. The
section ordering is the part that is easy to get backwards.

### Where the filter goes, and the number that falls out

Not before the resampler. Filtering **after** it, at the decode rate, is better
for three reasons:

1. A filter is inherently safe in place -- one sample in, one sample out -- so
   it needs no second buffer and cannot hit the upsampling aliasing hazard.
2. It leaves the existing 250 kHz anti-alias path completely untouched.
3. The normalised cutoff is then **the same number for every bandwidth**:

```
cutoff / decode_rate = (bandwidth / 2) / (2 x bandwidth) = 0.25   always
```

One coefficient set covers 125, 250 and 500 kHz. And at exactly a quarter of
the sample rate the bilinear transform gives `K = tan(pi/4) = 1`, so every
`a1` term is **exactly zero** -- an unusually well-conditioned filter:

```c
// 10th-order Butterworth, fc = 0.25 x fs. a1 is zero by construction.
{0.0028964459f, 0.0057928918f, 0.0028964459f, 0.0f, 0.00619395866f},
{1.0f, 2.0f, 1.0f, 0.0f, 0.0576378106f},
{1.0f, 2.0f, 1.0f, 0.0f, 0.171572875f},
{1.0f, 2.0f, 1.0f, 0.0f, 0.375524806f},
{1.0f, 2.0f, 1.0f, 0.0f, 0.729453817f},
```

| Frequency | Response | | Pole radii |
| --- | --- | --- | --- |
| 0.20 x fs | -0.01 dB | | 0.0787 |
| 0.25 x fs (channel edge) | -3.01 dB | | 0.2401 |
| 0.30 x fs | -27.76 dB | | 0.4142 |
| 0.35 x fs | -58.57 dB | | 0.6128 |
| 0.50 x fs (Nyquist) | -300 dB | | 0.8541 |

All stable, -3 dB at the channel edge -- the same character as the existing
filter has for its own channel.

### Why it is not shipped yet

Deliberately held until stage 4 reports. Adding a filter to a path whose basic
decode has not yet been seen working would confound the diagnosis: a silent
result would then have two possible causes instead of one. If stage 4 shows
preambles but failing headers, 2.74 dB is exactly the kind of margin that
decides it, and this goes in. The coefficients are derived and verified; what
is left is ~15 lines and a run of the DSP vector.

---

## The energy trigger: checked, and deliberately left alone

Nothing decodes unless `lora_channel_excess_db()` first clears
`kLoraChannelExcessMinDb = -4.0`. That metric sums 4 consecutive IQ samples --
a boxcar whose first null is at 960/4 = **240 kHz**. It was designed against a
250 kHz channel (+/- 125 kHz), which sits comfortably inside that. A 500 kHz
channel spans +/- 250 kHz and runs *past* the null, so part of every sweep is
attenuated by the very filter meant to be measuring it. If that pushed a real
signal under the threshold, the trigger would go deaf and the entire 500 kHz
effort would be moot regardless of the decoder.

It does not. A strong chirp, by bandwidth:

| Channel | Metric reads | Margin over the -4.0 dB threshold |
| --- | --- | --- |
| 125 kHz | +5.72 dB | 9.7 dB |
| 250 kHz | +4.88 dB | 8.9 dB |
| **500 kHz** | **+2.49 dB** | **6.5 dB** |

2.39 dB worse off than 250 kHz, which is a smaller margin rather than a wall.

**Widening the boxcar to suit the wider channel is the obvious fix and it is
wrong.** A wider boxcar is a narrower filter in frequency, so it raises the
noise floor of the metric faster than it raises the signal:

| Taps | First null | Flat noise | 500 kHz chirp |
| --- | --- | --- | --- |
| 2 | 480 kHz | **-3.06 dB** | +2.07 dB |
| 3 | 320 kHz | -4.78 dB | +2.52 dB |
| **4 (current)** | 240 kHz | **-6.03 dB** | **+2.49 dB** |
| 8 | 120 kHz | -9.01 dB | +2.66 dB |

At 2 taps flat noise reads **above** the -4.0 threshold: noise alone would arm
a capture continuously, which is the dead-detector failure of 3c in reverse. At
3 taps it sits 0.78 dB under and would false-trigger on ordinary excursions.
Four taps gives the best separation of any width, for 500 kHz as well as 250,
and the modelled flat-noise figure (-6.03 dB) matches what 3c measured on air
(-5.5 dB median of 172). **Left unchanged.**

Confirmed independently on air before this work: a real 500 kHz node produced
triggers at -0.8 to -1.5 dB against the -4.0 threshold.

---

## What this does not address

- **Out-of-channel noise on the 500 kHz path.** With no filter, the full
  +/- 480 kHz of captured noise reaches the dechirper against a +/- 250 kHz
  channel -- roughly 2.6 dB of avoidable SNR loss, and no rejection of an
  adjacent-channel interferer. On 915 MHz ISM that is not academic; 3c exists
  because of interferers. A 250 kHz-cutoff filter for this path is stage 5,
  deliberately separate so stage 1 lands working on its own.
- ~~**Coding rate 4/8.**~~ Done -- see stage 6. It needed no new code.
- **The energy trigger** was checked and left alone -- see the section above.
  It reads 2.39 dB lower on a 500 kHz channel but keeps 6.5 dB of margin, and
  every wider boxcar is worse.
