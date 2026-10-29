#!/usr/bin/env python3
"""Drives the Tab5's WWV tick measurement (`FT8 WWV`) on one or more frequencies and prints what it hears.

WWV (Fort Collins) sends a 5 ms burst of 1000 Hz at the start of every second on 2.5, 5, 10, 15 and 20 MHz. The firmware tunes the receiver
1 kHz below the carrier (so the carrier is a steady line near 1000 Hz and is used as the phase reference), folds the tick over one second
for about a minute and reports where it falls inside the Tab5's own second. The band is put back afterwards. See wwv_tick.hpp.

  python tools/wwv_probe.py --port COM39                      listen on 5, 10 and 15 MHz, 90 s each
  python tools/wwv_probe.py --port COM39 --freqs 10 --seconds 120
  python tools/wwv_probe.py --port COM39 --freqs 10 --cal     after `FT8 NTP`: learn this site's fixed delay for 10 MHz
  python tools/wwv_probe.py --port COM39 --freqs 10 --sync    correct the clock from the tick (needs the calibration for that frequency)

The decoder must be running (FT8 RUN 1, or the FT8 screen open). Pairs from the same key file as tab5_ft8.py; opening the port does not reset
the board. The signal analysis itself runs on the device and is tested on the host in tests/wwv_tick_tests.cpp.
"""
import argparse
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import tab5_ft8 as t  # noqa: E402


def run_one(dev, key, mhz, seconds, mode):
    dev.authenticate(key)
    suffix = (" " + mode.upper()) if mode else ""
    dev.send("FT8 WWV %g %d%s" % (mhz, seconds, suffix))
    lines = dev.read_until(("ORC_FT8_WWV_START", "ORC_FT8_ERROR"), 10, echo=False)
    start = next((l for l in lines if "ORC_FT8_WWV_START" in l or "ORC_FT8_ERROR" in l), "")
    if "ORC_FT8_WWV_START" not in start:
        print("%g MHz: could not start: %s" % (mhz, start[start.find("ORC_"):] if start else "no reply"))
        return None
    data = None
    lines = dev.read_until(("ORC_FT8_WWV_RESULT",), seconds + 90, echo=False)
    for line in lines:
        if "ORC_FT8_WWV mhz=" in line:
            data = dict(re.findall(r"(\w+)=(-?[0-9.]+)", line[line.find("ORC_FT8_WWV mhz="):]))
    verdict = next((l[l.find("ORC_FT8_WWV_RESULT"):] for l in lines if "ORC_FT8_WWV_RESULT" in l), "no result (the measurement did not finish)")
    if data:
        print("%g MHz: carrier %s Hz (%s dB over noise), %s s folded, ticks=%s, tick at %s ms in the Tab5's second, z=%s, even/odd seconds agree=%s"
              % (mhz, data.get("carrier_hz"), data.get("carrier_snr_db"), data.get("seconds"), data.get("ticks"), data.get("tick_wall_ms"),
                 data.get("z"), data.get("agree")))
    print("        " + verdict)
    return data


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", required=True)
    ap.add_argument("--key")
    ap.add_argument("--freqs", type=float, nargs="+", default=[5, 10, 15], help="WWV frequencies in MHz (2.5 5 10 15 20)")
    ap.add_argument("--seconds", type=int, default=90, help="seconds of tick to fold per frequency (20-240)")
    group = ap.add_mutually_exclusive_group()
    group.add_argument("--cal", action="store_true", help="store the fixed delay for each frequency (needs a network sync in the last 15 minutes)")
    group.add_argument("--sync", action="store_true", help="correct the clock from the tick (needs the calibration)")
    a = ap.parse_args()
    mode = "cal" if a.cal else "sync" if a.sync else ""
    key = t.find_key(a.key)
    dev = t.Tab5(a.port)
    heard = [m for m in a.freqs if (run_one(dev, key, m, a.seconds, mode) or {}).get("ticks") == "1"]
    print("RESULT: WWV tick found on %s" % ", ".join("%g MHz" % m for m in heard) if heard else "RESULT: no WWV tick found on any frequency tried")


if __name__ == "__main__":
    main()
