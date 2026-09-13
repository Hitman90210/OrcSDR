#!/usr/bin/env python3
"""Measure power, channel occupancy, and repeated LoRa chirps in ORCIQ files."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).parents[1]))
from decode_orciq import read_capture


def downchirp(sf: int, bandwidth: int, rate: int) -> np.ndarray:
    samples = round(rate * (1 << sf) / bandwidth)
    time = np.arange(samples) / rate
    slope = -(bandwidth * bandwidth) / (1 << sf)
    phase = 2 * np.pi * (bandwidth * 0.5 + 0.5 * slope * time) * time
    return np.exp(1j * phase)


def chirp_metrics(signal: np.ndarray, sf: int, bandwidth: int, rate: int) -> dict:
    reference = downchirp(sf, bandwidth, rate)
    samples = reference.size
    fft_size = samples * 4
    previous = None
    consecutive = 0
    max_consecutive = 0
    peak_to_median = 0.0
    for start in range(0, signal.size - samples + 1, samples):
        spectrum = np.fft.fft(signal[start : start + samples] * reference, fft_size)
        half = fft_size // 2
        magnitude = np.abs(spectrum[:half]) + np.abs(spectrum[half:])
        peak = int(np.argmax(magnitude))
        peak_to_median = max(
            peak_to_median,
            float(magnitude[peak] / max(np.median(magnitude), 1e-12)),
        )
        if previous is None:
            consecutive = 1
        else:
            delta = abs(peak - previous)
            consecutive = consecutive + 1 if min(delta, half - delta) <= 4 else 1
        previous = peak
        max_consecutive = max(max_consecutive, consecutive)
    return {
        "max_consecutive": max_consecutive,
        "peak_to_median": round(peak_to_median, 3),
    }


def _resample(signal: np.ndarray, source_rate: int, target_rate: int) -> np.ndarray:
    output_size = signal.size * target_rate // source_rate
    position = np.arange(output_size, dtype=np.float64) * source_rate / target_rate
    first = position.astype(np.int64)
    fraction = position - first
    second = np.minimum(first + 1, signal.size - 1)
    return signal[first] * (1 - fraction) + signal[second] * fraction


def capture_metrics(path: Path) -> dict:
    rate, frequency, sf, bandwidth, raw = read_capture(path)
    values = np.frombuffer(raw, dtype=np.uint8).reshape(-1, 2).astype(np.float32)
    signal = ((values[:, 0] - 127.5) + 1j * (values[:, 1] - 127.5)) / 127.5
    block_size = 4096
    blocks = signal[: signal.size // block_size * block_size].reshape(-1, block_size)
    rms = np.sqrt(np.mean(np.abs(blocks) ** 2, axis=1))
    spectrum = np.fft.fftshift(np.fft.fft(blocks, axis=1), axes=1)
    frequencies = np.fft.fftshift(np.fft.fftfreq(block_size, 1 / rate))
    power = np.abs(spectrum) ** 2
    channel_ratio = power[:, np.abs(frequencies) <= bandwidth / 2].sum(axis=1) / np.maximum(
        power.sum(axis=1), 1e-20
    )
    decode_rate = bandwidth * 2
    chirps = chirp_metrics(_resample(signal, rate, decode_rate), sf, bandwidth, decode_rate)
    return {
        "path": str(path),
        "sample_rate": rate,
        "frequency_hz": frequency,
        "sf": sf,
        "bandwidth_hz": bandwidth,
        "raw_bytes": len(raw),
        "power_rms_p95": round(float(np.percentile(rms, 95)), 6),
        "power_rms_max": round(float(rms.max()), 6),
        "channel_ratio_p95": round(float(np.percentile(channel_ratio, 95)), 6),
        "channel_ratio_max": round(float(channel_ratio.max()), 6),
        **chirps,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("captures", nargs="+", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    report = {"captures": [capture_metrics(path) for path in args.captures]}
    text = json.dumps(report, indent=2) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(text, encoding="utf-8")
    print(text, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
