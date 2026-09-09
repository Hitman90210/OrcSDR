"""Stage 5: a channel filter for the 500 kHz path.

Without one, the whole +/- 480 kHz of captured noise reaches the dechirper
against a +/- 250 kHz channel. That is not merely a noise-floor question,
because dechirp_peak() folds the spectrum in half:

    magnitude = |X[b]| + |X[b + n/2]|

At a 1 MHz decode rate that pairs frequencies 500 kHz apart, so a component at
+400 kHz -- well outside the channel -- lands on the bin for -100 kHz, which is
inside it. Out-of-channel energy does not just sit in the background; the fold
puts it directly onto the peak search. On 915 MHz ISM, where 3c exists because
of interferers, that matters.

First reproduce kLoraLowpass from a Butterworth design to confirm what it is,
then generate the 250 kHz-cutoff sibling the same way.
"""
import math
import cmath

FS = 960000.0
ORDER = 10
SECTIONS = ORDER // 2

EXISTING = [
    (1.55166027e-05, 3.10332055e-05, 1.55166027e-05, -0.794469113, 0.162197278),
    (1.0, 2.0, 1.0, -0.828439251, 0.211890842),
    (1.0, 2.0, 1.0, -0.901782183, 0.319181301),
    (1.0, 2.0, 1.0, -1.02691495, 0.502233045),
    (1.0, 2.0, 1.0, -1.22708148, 0.795048706),
]


def butterworth(cutoff_hz, fs=FS, order=ORDER):
    """Cascaded biquads, bilinear transform, gain folded into section 0.

    Returns the same {b0, b1, b2, a1, a2} shape kLoraLowpass uses, ordered
    most-damped first, which is how the existing table is ordered.
    """
    k = math.tan(math.pi * cutoff_hz / fs)
    k2 = k * k
    pairs = []
    for i in range(1, order // 2 + 1):
        q = 1.0 / (2.0 * math.sin((2 * i - 1) * math.pi / (2 * order)))
        denom = 1.0 + k / q + k2
        pairs.append((k2 / denom,                      # b0 before gain folding
                      2.0 * (k2 - 1.0) / denom,        # a1
                      (1.0 - k / q + k2) / denom))     # a2
    pairs.sort(key=lambda p: p[2])          # ascending pole radius, as kLoraLowpass is
    gain = 1.0
    for b0, _, _ in pairs:
        gain *= b0
    out = []
    for index, (_, a1, a2) in enumerate(pairs):
        if index == 0:
            out.append((gain, 2.0 * gain, gain, a1, a2))
        else:
            out.append((1.0, 2.0, 1.0, a1, a2))
    return out


def response_db(sections, frequency, fs=FS):
    z = cmath.exp(2j * cmath.pi * frequency / fs)
    total = 1.0 + 0j
    for b0, b1, b2, a1, a2 in sections:
        total *= (b0 + b1 / z + b2 / (z * z)) / (1.0 + a1 / z + a2 / (z * z))
    m = abs(total)
    return 20.0 * math.log10(m) if m > 0 else -300.0


print("1. Is kLoraLowpass a 10th-order Butterworth at 125 kHz?")
guess = butterworth(125000.0)
worst = 0.0
for got, want in zip(guess, EXISTING):
    for g, w in zip(got, want):
        worst = max(worst, abs(g - w) / max(abs(w), 1e-12))
print("   worst relative coefficient difference: %.3e" % worst)
print("   ->", "yes, reproduced" if worst < 1e-3 else "no, different design")
print()

print("2. The 250 kHz-cutoff sibling for the 500 kHz channel")
new = butterworth(250000.0)
for index, (b0, b1, b2, a1, a2) in enumerate(new):
    print("   {%.9gf, %.9gf, %.9gf, %.9gf, %.9gf}," % (b0, b1, b2, a1, a2))
print()

print("   response:")
for khz in (0, 100, 200, 240, 250, 260, 300, 350, 400, 480):
    print("     %6.1f kHz  %+8.2f dB" % (khz, response_db(new, khz * 1000.0)))
print()

# What the fold actually costs. Everything from the channel edge to the capture
# edge folds onto in-channel bins, so integrate noise power there both ways.
def folded_noise_power(sections):
    unfiltered = filtered = 0.0
    steps = 2000
    for i in range(steps):
        f = 250000.0 + (480000.0 - 250000.0) * (i + 0.5) / steps
        gain = 10.0 ** (response_db(sections, f) / 10.0)
        unfiltered += 1.0
        filtered += gain
    return unfiltered / steps, filtered / steps


raw, filt = folded_noise_power(new)
in_channel = 500000.0
out_channel = 2.0 * (480000.0 - 250000.0)
print("3. What it buys")
print("   out-of-channel band that folds inward: %.0f kHz against a %.0f kHz channel"
      % (out_channel / 1000, in_channel / 1000))
before = 10.0 * math.log10((in_channel + out_channel) / in_channel)
after = 10.0 * math.log10((in_channel + out_channel * filt) / in_channel)
print("   noise raised by folding, unfiltered: %+.2f dB" % before)
print("   noise raised by folding, filtered  : %+.2f dB" % after)
print("   recovered: %.2f dB" % (before - after))
print()
print("   stability: max |pole| per section")
for index, (_, _, _, a1, a2) in enumerate(new):
    disc = a1 * a1 - 4.0 * a2
    if disc >= 0:
        roots = [(-a1 + math.sqrt(disc)) / 2.0, (-a1 - math.sqrt(disc)) / 2.0]
        mag = max(abs(r) for r in roots)
    else:
        mag = math.sqrt(a2)
    print("     section %d: %.6f %s" % (index, mag, "ok" if mag < 1.0 else "UNSTABLE"))
