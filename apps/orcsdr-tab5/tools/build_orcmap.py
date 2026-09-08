#!/usr/bin/env python3
"""Build an ORCMAP1 offline map for *your* area from OpenStreetMap data.

The signed data catalog only publishes one map pack (Lane County, Oregon --
the upstream author's home county), which is not useful anywhere else. This
builds the same runtime format for any bounding box, straight from the public
Overpass API, so the Tab5's radar and LoRa map views show local roads, water
and airports.

    python build_orcmap.py --bbox 47.40 -122.60 47.85 -122.05 --out local_map.idx

Copy the result to the SD card as /orcsdr/data/local_map.idx; the firmware
loads that in preference to the packaged pack.

The device holds a deliberately small map (640 segments, 32 labels) so it fits
in RAM alongside the DSP. Keep the box to roughly one metro area; the script
simplifies and thins to fit and tells you what it dropped.
"""

from __future__ import annotations

import argparse
import json
import math
import sys
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path

OVERPASS_URL = "https://overpass-api.de/api/interpreter"
SEGMENT_CAPACITY = 640
LABEL_CAPACITY = 32

# Only the classes the device renders, cheapest-to-draw first.
QUERY_TEMPLATE = """
[out:json][timeout:{timeout}];
(
  way["highway"~"^(motorway|trunk|primary)$"]({bbox});
  way["natural"="coastline"]({bbox});
  way["waterway"="river"]({bbox});
  way["natural"="water"]({bbox});
  way["aeroway"="runway"]({bbox});
  node["place"~"^(city|town)$"]({bbox});
);
out geom;
"""


def classify(tags: dict) -> str | None:
    if "highway" in tags:
        return "R"
    if tags.get("natural") in ("coastline", "water") or tags.get("waterway") == "river":
        return "W"
    if "aeroway" in tags:
        return "A"
    return None


def fetch(bbox: tuple[float, float, float, float], timeout: int) -> dict:
    query = QUERY_TEMPLATE.format(
        bbox=f"{bbox[0]},{bbox[1]},{bbox[2]},{bbox[3]}", timeout=timeout
    )
    request = urllib.request.Request(
        OVERPASS_URL,
        data=urllib.parse.urlencode({"data": query}).encode(),
        headers={"User-Agent": "OrcSDR-map-builder/1 (+https://github.com/hardcoreerik/OrcSDR)"},
    )
    with urllib.request.urlopen(request, timeout=timeout + 30) as response:
        return json.load(response)


def simplify(points: list[tuple[float, float]], tolerance_deg: float) -> list[tuple[float, float]]:
    """Ramer-Douglas-Peucker, plain lat/lon -- accurate enough at this scale."""
    if len(points) < 3:
        return points
    first, last = points[0], points[-1]
    dx, dy = last[1] - first[1], last[0] - first[0]
    span = math.hypot(dx, dy)
    worst_index, worst = 0, 0.0
    for index in range(1, len(points) - 1):
        point = points[index]
        if span == 0.0:
            distance = math.hypot(point[1] - first[1], point[0] - first[0])
        else:
            distance = abs(dx * (first[0] - point[0]) - dy * (first[1] - point[1])) / span
        if distance > worst:
            worst_index, worst = index, distance
    if worst <= tolerance_deg:
        return [first, last]
    left = simplify(points[: worst_index + 1], tolerance_deg)
    right = simplify(points[worst_index:], tolerance_deg)
    return left[:-1] + right


# Roads are what make a radar view readable, but a purely road-ordered fill
# spends the whole budget before reaching the coastline -- which is the most
# recognisable feature on a coastal map. Give each class its own share and let
# roads absorb whatever the others do not use.
BUDGET = {"R": 380, "W": 200, "A": 60}


def build(data: dict, tolerance_deg: float) -> tuple[list[str], dict[str, int], int]:
    ways: dict[str, list[list[tuple[float, float]]]] = {"R": [], "W": [], "A": []}
    labels: list[tuple[float, float, str]] = []
    for element in data.get("elements", []):
        tags = element.get("tags", {})
        if element.get("type") == "node":
            name = (tags.get("name") or "").strip()
            if name and name.isascii():
                labels.append((element["lat"], element["lon"], name[:23]))
            continue
        kind = classify(tags)
        geometry = element.get("geometry")
        if not kind or not geometry:
            continue
        points = [(node["lat"], node["lon"]) for node in geometry]
        if len(points) > 1:
            ways[kind].append(simplify(points, tolerance_deg))

    # Longest ways first within a class: they carry the most shape per segment.
    for entries in ways.values():
        entries.sort(key=len, reverse=True)

    records: list[str] = []
    counts = {"R": 0, "W": 0, "A": 0}
    spare = SEGMENT_CAPACITY - sum(BUDGET.values())

    def emit(kind: str, budget: int) -> None:
        for points in ways[kind]:
            for start, end in zip(points, points[1:]):
                if counts[kind] >= budget or sum(counts.values()) >= SEGMENT_CAPACITY:
                    return
                records.append(
                    f"{kind} {start[0]:.6f} {start[1]:.6f} {end[0]:.6f} {end[1]:.6f}\n"
                )
                counts[kind] += 1

    for kind in ("W", "A", "R"):
        emit(kind, BUDGET[kind])
    # Second pass: hand the unclaimed budget to roads.
    leftover = SEGMENT_CAPACITY - sum(counts.values())
    if leftover > 0:
        emit("R", BUDGET["R"] + leftover + spare)

    kept_labels = labels[:LABEL_CAPACITY]
    for lat, lon, name in kept_labels:
        records.append(f"L {lat:.6f} {lon:.6f} {name}\n")
    return records, counts, len(kept_labels)


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--bbox", nargs=4, type=float, required=True,
                        metavar=("SOUTH", "WEST", "NORTH", "EAST"),
                        help="bounding box in degrees, e.g. 47.40 -122.60 47.85 -122.05")
    parser.add_argument("--out", type=Path, default=Path("local_map.idx"))
    parser.add_argument("--tolerance", type=float, default=0.002,
                        help="simplification tolerance in degrees (default 0.002, ~200 m)")
    parser.add_argument("--timeout", type=int, default=120)
    parser.add_argument("--geojson", type=Path,
                        help="read a local Overpass JSON file instead of querying the API")
    args = parser.parse_args(argv)

    south, west, north, east = args.bbox
    if south >= north or west >= east:
        parser.error("bbox must be SOUTH WEST NORTH EAST with south < north and west < east")

    if args.geojson:
        data = json.loads(args.geojson.read_text(encoding="utf-8"))
    else:
        try:
            data = fetch((south, west, north, east), args.timeout)
        except urllib.error.URLError as error:
            print(f"Overpass request failed: {error}", file=sys.stderr)
            return 1

    records, counts, labels = build(data, args.tolerance)
    segments = sum(counts.values())
    if segments == 0:
        print("No renderable features in that box -- try a larger area.", file=sys.stderr)
        return 1

    temporary = args.out.with_suffix(args.out.suffix + ".tmp")
    try:
        with temporary.open("w", encoding="ascii", newline="\n") as stream:
            stream.write("ORCMAP1\n" + "".join(records))
        temporary.replace(args.out)
    finally:
        temporary.unlink(missing_ok=True)

    print(f"Wrote {args.out}: {segments}/{SEGMENT_CAPACITY} segments "
          f"({counts['R']} road, {counts['W']} water, {counts['A']} airport), "
          f"{labels}/{LABEL_CAPACITY} labels.")
    if segments >= SEGMENT_CAPACITY:
        print("Segment budget was reached -- raise --tolerance or shrink --bbox "
              "for a map that covers the whole box evenly.")
    print("Copy it to the SD card as /orcsdr/data/local_map.idx.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
