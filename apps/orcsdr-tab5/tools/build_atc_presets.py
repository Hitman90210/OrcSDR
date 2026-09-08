#!/usr/bin/env python3
"""Build ATC listening presets for *your* area.

The signed `faa_aviation` pack carries 40,000+ FAA frequency records but no
coordinates, so the firmware's nearest-ATC lookup has nothing to measure
distance against and "Listen to ATC" never finds anything. The `ATC` preset
lines it does parse are an optional section that pack does not include.

This fills the gap from OurAirports (public domain), which publishes airport
coordinates and airport frequencies in two joinable CSVs:

    python build_atc_presets.py --near 38.6582 -77.2497 --out local_atc.idx

Copy the result to the SD card as /orcsdr/data/local_atc.idx; the firmware
loads that in preference to the packaged pack.

The device holds 24 presets and its nearest() picks the single closest one, so
this emits **one frequency per airport** -- the most useful control frequency
at each field -- for the nearest airports. That way "nearest ATC" means "the
tower at the closest airfield" rather than an arbitrary row.

Data: OurAirports (https://ourairports.com/data/), public domain.
"""

from __future__ import annotations

import argparse
import csv
import io
import math
import sys
import urllib.error
import urllib.request
from pathlib import Path

BASE = "https://davidmegginson.github.io/ourairports-data"
AIRPORTS_URL = f"{BASE}/airports.csv"
FREQUENCIES_URL = f"{BASE}/airport-frequencies.csv"

# Firmware limits: atc_presets.hpp kCapacity, and label[32] parsed as %31[^\n].
MAX_PRESETS = 24
MAX_LABEL = 31

# VHF airband the RTL-SDR can actually tune for voice.
MIN_MHZ, MAX_MHZ = 118.0, 137.0

# Preference order when one airport publishes several frequencies. Tower first:
# it is what someone means by "listen to the airport".
TYPE_RANK = ["TWR", "CTAF", "APP", "A/D", "ARR", "DEP", "GND", "ATIS", "AWOS",
             "CLD", "UNIC"]


def fetch(url: str, timeout: int) -> str:
    request = urllib.request.Request(
        url, headers={"User-Agent": "OrcSDR-atc-builder/1"})
    with urllib.request.urlopen(request, timeout=timeout) as response:
        return response.read().decode("utf-8", "replace")


def load(url: str, local: Path | None, timeout: int) -> list[dict]:
    text = local.read_text(encoding="utf-8") if local else fetch(url, timeout)
    return list(csv.DictReader(io.StringIO(text)))


def rank(kind: str) -> int:
    kind = (kind or "").upper()
    for index, name in enumerate(TYPE_RANK):
        if kind.startswith(name):
            return index
    return len(TYPE_RANK)


def nautical_miles(lat1: float, lon1: float, lat2: float, lon2: float) -> float:
    dlat = (lat2 - lat1) * 60.0
    dlon = (lon2 - lon1) * 60.0 * math.cos(math.radians(lat1))
    return math.hypot(dlat, dlon)


def build(airports: list[dict], frequencies: list[dict],
          home: tuple[float, float], radius_nm: float) -> list[tuple]:
    by_ident = {}
    for row in airports:
        if not row.get("latitude_deg") or not row.get("longitude_deg"):
            continue
        by_ident[row["ident"]] = row

    # Keep the single best frequency per airport.
    best: dict[str, tuple] = {}
    for row in frequencies:
        airport = by_ident.get(row.get("airport_ident", ""))
        if not airport:
            continue
        try:
            mhz = float(row["frequency_mhz"])
            lat = float(airport["latitude_deg"])
            lon = float(airport["longitude_deg"])
        except (TypeError, ValueError):
            continue
        if not MIN_MHZ <= mhz <= MAX_MHZ:
            continue
        distance = nautical_miles(home[0], home[1], lat, lon)
        if distance > radius_nm:
            continue
        ident = airport["ident"]
        candidate = (rank(row.get("type")), distance, ident, lat, lon, mhz,
                     (row.get("type") or "").upper())
        if ident not in best or candidate[0] < best[ident][0]:
            best[ident] = candidate

    # Nearest airports first, capped at what the device can hold.
    chosen = sorted(best.values(), key=lambda c: c[1])[:MAX_PRESETS]
    return chosen


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--near", nargs=2, type=float, required=True,
                        metavar=("LAT", "LON"),
                        help="your receiver location in decimal degrees")
    parser.add_argument("--radius", type=float, default=60.0,
                        help="search radius in nautical miles (default 60)")
    parser.add_argument("--out", type=Path, default=Path("local_atc.idx"))
    parser.add_argument("--timeout", type=int, default=120)
    parser.add_argument("--airports-csv", type=Path,
                        help="use a local airports.csv instead of downloading")
    parser.add_argument("--frequencies-csv", type=Path,
                        help="use a local airport-frequencies.csv instead")
    args = parser.parse_args(argv)

    try:
        airports = load(AIRPORTS_URL, args.airports_csv, args.timeout)
        frequencies = load(FREQUENCIES_URL, args.frequencies_csv, args.timeout)
    except urllib.error.URLError as error:
        print(f"OurAirports download failed: {error}", file=sys.stderr)
        return 1

    chosen = build(airports, frequencies, (args.near[0], args.near[1]), args.radius)
    if not chosen:
        print(f"No airband frequencies within {args.radius:.0f} NM.", file=sys.stderr)
        return 1

    lines = ["ORCATC1\n"]
    for _, distance, ident, lat, lon, mhz, kind in chosen:
        label = f"{ident} {kind}"[:MAX_LABEL]
        lines.append(f"ATC {round(lat * 1e7)} {round(lon * 1e7)} "
                     f"{round(mhz * 1e6)} {label}\n")

    temporary = args.out.with_suffix(args.out.suffix + ".tmp")
    try:
        with temporary.open("w", encoding="ascii", newline="\n") as stream:
            stream.writelines(lines)
        temporary.replace(args.out)
    finally:
        temporary.unlink(missing_ok=True)

    print(f"Wrote {args.out}: {len(chosen)}/{MAX_PRESETS} presets "
          f"within {args.radius:.0f} NM.")
    for _, distance, ident, _, _, mhz, kind in chosen[:8]:
        print(f"  {distance:5.1f} NM  {ident:6s} {kind:5s} {mhz:7.3f} MHz")
    if len(chosen) > 8:
        print(f"  ... and {len(chosen) - 8} more")
    print("Copy it to the SD card as /orcsdr/data/local_atc.idx.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
