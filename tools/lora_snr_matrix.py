#!/usr/bin/env python3
"""Grade a recorded LoRa capture by adding calibrated noise, and decode each step.

Handoff item 5 asks a question no amount of bench noise-floor work answers: not
"does the receiver reject noise", but "how weak can a real signal get before it
stops working". One clean ORCIQ capture answers it offline. This builds a ladder
of impaired copies at chosen signal-to-noise ratios and decodes each one, so the
result is the SNR where decoding actually stops.

What this does and does not measure, because the distinction decides what the
number is worth:

- It measures the **host** decoder, `tools/decode_orciq.py` (lora_phy). It does
  not measure the Tab5's native decoder, and it says nothing directly about the
  channel scanner's trigger threshold.
- It is still the cheapest bound available: nothing transmits, no device is
  needed, and the impaired captures it writes are exactly the corpus an
  on-device replay harness would later feed to the native decoder.

Noise is added, never removed, so every target must sit below the capture's own
measured SNR. Targets above it are reported as skipped rather than silently
producing an easier case than the recording.

Dependencies are the repo's documented LoRa set, which decode_orciq.py needs
anyway. They must be installed under **Python 3.11 or 3.12**: the pinned
numpy<2 raises OverflowError on import under 3.13+, so a 3.14 default
interpreter cannot run this even with the packages installed.

    C:\\Espressif\\tools\\python\\python.exe -m venv .local/venv-lora
    .local/venv-lora/Scripts/python.exe -m pip install -r tools/requirements-lora.txt
    .local/venv-lora/Scripts/python.exe tools/lora_snr_matrix.py --self-check

`.local/` is gitignored, which is also where captures and impaired copies live:
raw IQ does not belong in the repository.

Examples:
    python tools/lora_snr_matrix.py --self-check
    python tools/lora_snr_matrix.py .local/lora-captures/*.orciq --snr 20 15 10 5
"""

from __future__ import annotations

import argparse
import json
import math
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from decode_orciq import HEADER, MAGIC, decode_capture, read_capture

# A LoRa packet occupies a small part of a capture, so levels are measured over
# short windows: the quiet ones are the noise floor, the loud ones the signal.
WINDOW_MS = 2.0
NOISE_PERCENTILE = 20.0
ACTIVE_PERCENTILE = 99.0


def _numpy():
    try:
        import numpy as np
    except Exception as error:  # noqa: BLE001 - a broken install fails here too
        # Not only ImportError: requirements-lora.txt pins numpy<2, and numpy 1.x
        # raises OverflowError on import under Python 3.13+, so the interpreter
        # matters as much as the package.
        raise SystemExit(
            f"numpy unusable on this interpreter ({sys.version.split()[0]}): {error}\n"
            "Use Python 3.11 or 3.12 and run: "
            "python -m pip install -r tools/requirements-lora.txt"
        ) from error
    return np


def to_complex(raw: bytes):
    """CU8 bytes to a complex signal, matching decode_orciq's scaling."""
    np = _numpy()
    iq = np.frombuffer(raw, dtype=np.uint8).astype(np.float64).reshape(-1, 2) - 127.5
    return (iq[:, 0] + 1j * iq[:, 1]) / 127.5


def to_cu8(signal) -> tuple[bytes, float]:
    """Complex signal back to CU8, reporting the fraction of clipped samples."""
    np = _numpy()
    interleaved = np.empty(signal.size * 2, dtype=np.float64)
    interleaved[0::2] = signal.real
    interleaved[1::2] = signal.imag
    scaled = interleaved * 127.5 + 127.5
    clipped = float(np.count_nonzero((scaled < 0) | (scaled > 255))) / scaled.size
    return np.clip(np.rint(scaled), 0, 255).astype(np.uint8).tobytes(), clipped


def measure_levels(signal, sample_rate: int) -> dict:
    """Separate signal power from noise power using per-window power percentiles."""
    np = _numpy()
    window = max(1, int(sample_rate * WINDOW_MS / 1000.0))
    usable = (signal.size // window) * window
    if usable < window * 8:
        raise ValueError("capture is too short to measure a noise floor")
    power = (np.abs(signal[:usable]) ** 2).reshape(-1, window).mean(axis=1)
    noise_power = float(np.percentile(power, NOISE_PERCENTILE))
    active_power = float(np.percentile(power, ACTIVE_PERCENTILE))
    # The loud windows carry signal *and* noise; the signal alone is the excess.
    signal_power = max(active_power - noise_power, 1e-20)
    duty = float(np.count_nonzero(power > noise_power * 4.0)) / power.size
    return {
        "noise_power": max(noise_power, 1e-20),
        "signal_power": signal_power,
        "snr_db": 10.0 * math.log10(signal_power / max(noise_power, 1e-20)),
        "active_duty": duty,
        "windows": int(power.size),
    }


def impair(signal, levels: dict, target_snr_db: float, seed: int):
    """Add independent complex Gaussian noise until the SNR reaches the target."""
    np = _numpy()
    wanted_noise_power = levels["signal_power"] / (10.0 ** (target_snr_db / 10.0))
    extra = wanted_noise_power - levels["noise_power"]
    if extra <= 0:
        raise ValueError(
            f"target {target_snr_db:.1f} dB is at or above the capture's own "
            f"{levels['snr_db']:.1f} dB; noise can only be added"
        )
    rng = np.random.default_rng(seed)
    sigma = math.sqrt(extra / 2.0)
    noise = rng.normal(0.0, sigma, signal.size) + 1j * rng.normal(0.0, sigma, signal.size)
    return signal + noise


def write_capture(path: Path, rate: int, freq: int, sf: int, bw: int, data: bytes) -> None:
    header = HEADER.pack(MAGIC, HEADER.size, rate, freq, len(data), 1, sf, 0, bw, 0)
    path.write_bytes(header + data)


def decode(path: Path) -> dict:
    """Run the host decoder, flattening its outcomes into one comparable record."""
    try:
        results = decode_capture(path)
    except ValueError as error:  # raised as "no LoRa preamble/sync found"
        return {"decoded": False, "reason": str(error).split("(")[0].strip(), "packets": []}
    except Exception as error:  # noqa: BLE001 - any decoder failure is a data point
        return {"decoded": False, "reason": f"{type(error).__name__}: {error}", "packets": []}
    packets = []
    errors: list[str] = []
    for result in results:
        if "error" in result:
            # The rejection text is the point, not the count: "long interleaver
            # CR 4/5 detected" and "no preamble" are completely different faults.
            reason = str(result["error"])
            if reason not in errors:
                errors.append(reason)
            continue
        payload = result.get("payload", b"")
        packets.append(
            {
                "id": f"{result.get('id', 0):08x}",
                "from": f"{result.get('from', 0):08x}",
                "port": result.get("port"),
                "cfo_hz": round(float(result.get("cfo_hz", 0.0)), 1),
                "text": payload.decode("utf-8", errors="replace")
                if result.get("port") in {1, 10, 11, 13, 32, 66}
                else payload.hex()[:48],
            }
        )
    return {
        "decoded": bool(packets),
        "reason": "" if packets else ("; ".join(errors) if errors else "no candidates"),
        "rejections": errors,
        "packets": packets,
    }


def grade(capture: Path, targets: list[float], seed: int, out_dir: Path, keep: bool,
          baseline_only: bool = False) -> dict:
    np = _numpy()
    rate, freq, sf, bw, raw = read_capture(capture)
    signal = to_complex(raw)
    levels = measure_levels(signal, rate)
    duration_s = signal.size / rate

    print(f"\n{capture.name}")
    print(
        f"  SF{sf} BW {bw} Hz at {rate} S/s, {duration_s:.1f} s, "
        f"measured SNR {levels['snr_db']:.1f} dB, active {levels['active_duty'] * 100:.1f}% of windows"
    )
    # With no window standing clear of the floor there is no burst to measure
    # against, so the SNR above is the spread of the noise itself, not a signal.
    quiet = levels["active_duty"] < 0.005
    if quiet:
        print("  WARNING: no burst found above the noise floor -- treat the SNR above as "
              "unreliable, and the ladder below as ungrounded")
    baseline = decode(capture)
    print(f"  as recorded: {'DECODED' if baseline['decoded'] else 'no decode'}"
          + (f" -- {baseline['packets'][0]['text'][:48]}" if baseline["packets"] else
             f" ({baseline['reason']})"))
    if not baseline["decoded"]:
        print("  NOTE: this capture does not decode as recorded, so a degradation ladder "
              "from it measures nothing. Grade a capture that decodes first.")
    if baseline_only:
        return {
            "capture": capture.name,
            "sf": sf,
            "bandwidth_hz": bw,
            "measured": {k: round(v, 6) if isinstance(v, float) else v for k, v in levels.items()},
            "estimator_unreliable": quiet,
            "baseline": baseline,
            "steps": [],
        }

    out_dir.mkdir(parents=True, exist_ok=True)
    steps = []
    for target in sorted(targets, reverse=True):
        try:
            impaired = impair(signal, levels, target, seed)
        except ValueError as error:
            print(f"  {target:6.1f} dB  skipped: {error}")
            steps.append({"target_snr_db": target, "skipped": str(error)})
            continue
        data, clipped = to_cu8(impaired)
        measured = measure_levels(to_complex(data), rate)
        path = out_dir / f"{capture.stem}_snr{target:+.0f}dB.orciq"
        write_capture(path, rate, freq, sf, bw, data)
        outcome = decode(path)
        if not keep:
            path.unlink(missing_ok=True)
        verdict = "DECODED" if outcome["decoded"] else "no decode"
        detail = outcome["packets"][0]["text"][:40] if outcome["packets"] else outcome["reason"]
        print(
            f"  {target:6.1f} dB  measured {measured['snr_db']:6.1f} dB  "
            f"clipped {clipped * 100:5.2f}%  {verdict:<9} {detail}"
        )
        steps.append(
            {
                "target_snr_db": target,
                "measured_snr_db": round(measured["snr_db"], 2),
                "clipped_fraction": round(clipped, 6),
                "decoded": outcome["decoded"],
                "reason": outcome["reason"],
                "packets": outcome["packets"],
            }
        )

    decoded = [s["target_snr_db"] for s in steps if s.get("decoded")]
    if decoded:
        print(f"  lowest SNR still decoded: {min(decoded):.1f} dB")
    return {
        "capture": capture.name,
        "sf": sf,
        "bandwidth_hz": bw,
        "sample_rate_sps": rate,
        "center_frequency_hz": freq,
        "duration_s": round(duration_s, 3),
        "measured": {k: round(v, 6) if isinstance(v, float) else v for k, v in levels.items()},
        "baseline": baseline,
        "seed": seed,
        "steps": steps,
        "lowest_decoded_snr_db": min(decoded) if decoded else None,
    }


def self_check() -> int:
    """Prove the instrument on a synthetic burst, with no capture and no decoder."""
    np = _numpy()
    failures = []
    rate = 960_000
    samples = rate // 2
    rng = np.random.default_rng(1)

    # A quiet channel with one loud burst, which is the shape the estimator expects.
    signal = (rng.normal(0, 0.002, samples) + 1j * rng.normal(0, 0.002, samples))
    burst = slice(samples // 3, samples // 3 + samples // 10)
    tone = np.exp(2j * np.pi * 50_000 * np.arange(samples // 10) / rate)
    signal[burst] += tone * 0.20

    levels = measure_levels(signal, rate)
    print(f"synthetic capture measures {levels['snr_db']:.1f} dB")

    for target in (20.0, 10.0, 3.0, 0.0, -5.0):
        impaired = impair(signal, levels, target, seed=7)
        measured = measure_levels(impaired, rate)["snr_db"]
        ok = abs(measured - target) <= 1.0
        print(f"  target {target:6.1f} dB -> measured {measured:6.1f} dB  {'ok' if ok else 'FAIL'}")
        if not ok:
            failures.append(f"target {target} produced {measured:.2f}")

    # Same seed must be reproducible; a different seed must not be.
    a = impair(signal, levels, 5.0, seed=11)
    b = impair(signal, levels, 5.0, seed=11)
    c = impair(signal, levels, 5.0, seed=12)
    if not np.array_equal(a, b):
        failures.append("same seed produced different noise")
    if np.array_equal(a, c):
        failures.append("different seeds produced identical noise")

    # A target above the capture's own SNR must be refused, not silently faked.
    try:
        impair(signal, levels, levels["snr_db"] + 6.0, seed=3)
        failures.append("a target above the capture SNR was accepted")
    except ValueError:
        pass

    # The CU8 round trip must preserve the level the decoder will see.
    data, clipped = to_cu8(impair(signal, levels, 6.0, seed=5))
    round_trip = measure_levels(to_complex(data), rate)["snr_db"]
    if abs(round_trip - 6.0) > 1.0:
        failures.append(f"CU8 round trip moved 6.0 dB to {round_trip:.2f}")
    if clipped > 0.01:
        failures.append(f"CU8 round trip clipped {clipped * 100:.2f}% of samples")
    print(f"  CU8 round trip at 6 dB -> {round_trip:.1f} dB, clipped {clipped * 100:.3f}%")

    if failures:
        print("\nSELF_CHECK_FAILED")
        for failure in failures:
            print(f"  - {failure}")
        return 1
    print("\nLORA_SNR_MATRIX_SELF_CHECK_OK")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("captures", nargs="*", type=Path, help="ORCIQ captures to grade")
    parser.add_argument("--snr", type=float, nargs="+", default=[20, 15, 10, 5, 0],
                        help="target SNRs in dB (default: 20 15 10 5 0)")
    parser.add_argument("--seed", type=int, default=1789, help="noise seed, for reproducibility")
    parser.add_argument("--out-dir", type=Path, default=Path(".local/lora-snr"),
                        help="where impaired captures and the report are written")
    parser.add_argument("--keep", action="store_true",
                        help="keep the impaired captures instead of deleting each after decoding")
    parser.add_argument("--json", type=Path, help="write the full report to this file")
    parser.add_argument("--self-check", action="store_true",
                        help="prove the impairment math without a capture or the decoder")
    parser.add_argument("--baseline-only", action="store_true",
                        help="measure and decode each capture as recorded, and stop there; "
                             "use it to find a capture worth grading")
    args = parser.parse_args()

    if args.self_check:
        return self_check()
    if not args.captures:
        parser.error("give at least one capture, or --self-check")

    report = [grade(c, args.snr, args.seed, args.out_dir, args.keep, args.baseline_only)
              for c in args.captures]
    if args.baseline_only:
        decoded = [r["capture"] for r in report if r["baseline"]["decoded"]]
        print(f"\n{len(decoded)} of {len(report)} captures decode as recorded"
              + (": " + ", ".join(decoded) if decoded else
                 " -- nothing here can anchor a degradation ladder"))
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(json.dumps(report, indent=2), encoding="utf-8")
        print(f"\nreport written to {args.json}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
