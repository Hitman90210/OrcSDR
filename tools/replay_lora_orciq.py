#!/usr/bin/env python3
"""Upload an ORCIQ capture to Tab5 PSRAM and run the native LoRa decoder."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import time
from collections import Counter
from pathlib import Path

import decode_orciq


def _wait_line(connection, prefixes, timeout=15):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        line = connection.readline().decode("utf-8", "replace").strip()
        if any(line.startswith(prefix) for prefix in prefixes):
            return line
    raise TimeoutError(f"Timed out waiting for {prefixes}")


def _upload_iq(connection, iq, *, rate, frequency_hz, sf, bandwidth_hz):
    digest = hashlib.sha256(iq).hexdigest()
    connection.write(
        f"RTL_LORA_REPLAY_BEGIN {len(iq)} {digest} {rate} {frequency_hz} {sf} "
        f"{bandwidth_hz}\n".encode("ascii")
    )
    ready = _wait_line(connection, ("RTL_LORA_REPLAY_READY", "RTL_LORA_REPLAY_ERROR"))
    if ready.startswith("RTL_LORA_REPLAY_ERROR"):
        raise RuntimeError(ready)
    chunk = int(re.search(r"\bchunk=(\d+)", ready).group(1))
    sent = 0
    while sent < len(iq):
        data = iq[sent : sent + chunk]
        connection.write(f"RTL_LORA_REPLAY_CHUNK {len(data)}\n".encode("ascii"))
        state = _wait_line(connection, ("RTL_LORA_REPLAY_DATA", "RTL_LORA_REPLAY_ERROR"))
        if state.startswith("RTL_LORA_REPLAY_ERROR"):
            raise RuntimeError(state)
        connection.write(data)
        connection.flush()
        sent += len(data)
        state = _wait_line(
            connection,
            ("RTL_LORA_REPLAY_ACK", "RTL_LORA_REPLAY_QUEUED", "RTL_LORA_REPLAY_ERROR"),
        )
        if state.startswith("RTL_LORA_REPLAY_ERROR"):
            raise RuntimeError(state)
        if sent < len(iq) and state != f"RTL_LORA_REPLAY_ACK bytes={sent}":
            raise RuntimeError(state)
    if state != f"RTL_LORA_REPLAY_QUEUED bytes={len(iq)}":
        raise RuntimeError(state)


def _read_traces(connection, done):
    if re.search(r"\bpreambles=0\b", done):
        return []
    traces = []
    while True:
        line = _wait_line(connection,
                          ("RTL_LORA_NATIVE_TRACE", "RTL_LORA_NATIVE_PREPROCESS",
                           "RTL_LORA_NATIVE_FFT", "RTL_LORA_NATIVE_SYMBOLS"), 15)
        traces.append(line)
        if line.startswith("RTL_LORA_NATIVE_SYMBOLS"):
            return traces


def _symbol_difference(reference, native):
    differences = [actual - expected for expected, actual in zip(reference, native)]
    histogram = Counter(differences)
    return {
        "first": next((i for i, difference in enumerate(differences) if difference), None),
        "different": sum(difference != 0 for difference in differences),
        "largest": max(differences, key=abs, default=0),
        "histogram": {
            (f"{difference:+d}" if difference else "0"): count
            for difference, count in sorted(histogram.items())
        },
    }


def _host_reference_symbols(path):
    np, _, _, _, LoRaReceiver, _ = decode_orciq._dependencies()
    from lora_phy.errors import NoPreambleError

    rate, frequency_hz, sf, bandwidth_hz, raw = decode_orciq.read_capture(path)
    iq = np.frombuffer(raw, dtype=np.uint8).astype(np.float32).reshape(-1, 2) - 127.5
    signal = (iq[:, 0] + 1j * iq[:, 1]) / 127.5

    def valid_symbols(candidate):
        receiver = LoRaReceiver(frequency_hz, sf, bandwidth_hz, rate,
                                has_header=True, preamble_len=16)
        groups, cfos, _ = receiver.demodulate(candidate)
        for symbols in groups:
            data, calculated_crc = receiver.decode(symbols)
            if not calculated_crc or bytes(data[-2:]) == bytes(calculated_crc):
                return symbols.tolist(), cfos
        return None, cfos

    try:
        symbols, cfos = valid_symbols(signal)
        if symbols is not None:
            return symbols
        samples = np.arange(len(signal), dtype=np.float64)
        for correction_hz in sorted({float(np.median(cfos)), *map(float, cfos)}, key=abs)[:8]:
            if abs(correction_hz) < 0.5:
                continue
            corrected = signal * np.exp(samples * (-2j * np.pi * correction_hz / rate))
            symbols, _ = valid_symbols(corrected)
            if symbols is not None:
                return symbols
    except NoPreambleError:
        pass
    return None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", type=Path)
    parser.add_argument("--port", default="COM17")
    parser.add_argument("--pairing-key", type=Path,
                        default=Path(__file__).resolve().parents[1] / ".orclink" / "ui-doc.key")
    parser.add_argument("--compare-host", action="store_true")
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    rate, frequency_hz, sf, bandwidth_hz, iq = decode_orciq.read_capture(args.capture)

    from help_media import Tab5
    tab5 = Tab5(args.port, args.pairing_key)
    try:
        tab5.authenticate()
        tab5.send("RTL_STOP")
        stopped = tab5.wait(("RTL_STOP_RESULT", "RTL_STOP_ERROR"), 20)
        if stopped != "RTL_STOP_RESULT ESP_OK":
            raise RuntimeError(stopped)
        _upload_iq(tab5.serial, iq, rate=rate, frequency_hz=frequency_hz, sf=sf,
                   bandwidth_hz=bandwidth_hz)
        done = _wait_line(tab5.serial, ("RTL_LORA_NATIVE_DONE",), 180)
        profile = _wait_line(tab5.serial, ("RTL_LORA_NATIVE_PROFILE",), 15)
        traces = _read_traces(tab5.serial, done)
        print(done)
        print(profile)
        for trace in traces:
            print(trace.encode("ascii", "backslashreplace").decode("ascii"))
        report = {"capture": str(args.capture), "done": done, "profile": profile,
                  "traces": traces}
        if args.compare_host:
            reference = _host_reference_symbols(args.capture)
            symbol_line = next((line for line in traces
                                if line.startswith("RTL_LORA_NATIVE_SYMBOLS")), None)
            native = ([int(value) for value in symbol_line.split("values=", 1)[1].split(",")]
                      if symbol_line else None)
            comparison = (_symbol_difference(reference, native)
                          if reference is not None and native is not None else None)
            report["symbol_difference"] = comparison
            print("RTL_LORA_SYMBOL_DIFF " + json.dumps(comparison, sort_keys=True))
        if args.report:
            args.report.parent.mkdir(parents=True, exist_ok=True)
            args.report.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    finally:
        tab5.close()


if __name__ == "__main__":
    main()
