#!/usr/bin/env python3
"""The FRS/GMRS and marine channel plans, checked against the band table.

Three claims are tested here, because the comment in band_profile.cpp asserts
the first and the firmware depends on the other two.

1. The raster each profile declares lands exactly on every channel of its
   service. That is what makes Control::channelized correct for the whole band
   rather than only part of it, and it is arithmetic, so it is checked by
   arithmetic -- against the origin and step read out of band_profile.cpp, not
   against a copy of them kept here. An earlier version of this test kept its
   own copy and checked the source only with `assertIn("462550000", entry)`,
   which the profile's low_hz satisfied on its own: moving raster_origin_hz off
   the channel grid left the test passing. Mutation testing found that, so the
   fields are now parsed positionally.

2. Channels 8-14 are the 467 MHz interstitials, limited to 12.5 kHz by
   47 CFR 95.1773 while the rest of the plan runs wider. The receiver's default
   filter has to follow the channel, or the neighbouring one bleeds in.

3. That narrowband window stays inside the GMRS band. rtl_filter_default_hz()
   tests the frequency before it looks at the band, so a window reaching past
   the top of the band would narrow the filter for frequencies that are not
   GMRS at all.
"""

from __future__ import annotations

import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
UI = ROOT / "apps" / "orcsdr-tab5" / "ui"

# 47 CFR 95.563 / 95.1763, in channel order.
FRS_GMRS_CHANNELS_HZ = {
    1: 462_562_500, 2: 462_587_500, 3: 462_612_500, 4: 462_637_500,
    5: 462_662_500, 6: 462_687_500, 7: 462_712_500,
    8: 467_562_500, 9: 467_587_500, 10: 467_612_500, 11: 467_637_500,
    12: 467_662_500, 13: 467_687_500, 14: 467_712_500,
    15: 462_550_000, 16: 462_575_000, 17: 462_600_000, 18: 462_625_000,
    19: 462_650_000, 20: 462_675_000, 21: 462_700_000, 22: 462_725_000,
}
NARROWBAND_CHANNELS = range(8, 15)

# A sample of the US VHF marine plan, which is a 25 kHz raster from 156.000.
MARINE_CHANNELS_HZ = {
    6: 156_300_000, 13: 156_650_000, 16: 156_800_000, 22: 157_100_000,
    68: 156_425_000, 72: 156_625_000, 88: 157_425_000,
}

# Profile, in declaration order (band_profile.hpp).
FIELDS = ("id", "name", "low_hz", "high_hz", "mode", "confidence", "control",
          "raster_origin_hz", "step_count", "default_step", "steps_hz",
          "filter", "owner")


def split_fields(entry: str) -> list[str]:
    """Top-level commas only, so the braced step list stays one field."""
    out, depth, quoted, current = [], 0, False, ""
    for ch in entry:
        if ch == '"':
            quoted = not quoted
        if not quoted:
            if ch in "{[(":
                depth += 1
            elif ch in "}])":
                depth -= 1
            if ch == "," and depth == 1:
                out.append(current.strip())
                current = ""
                continue
        current += ch
    tail = current.strip()
    if tail:
        out.append(tail)
    return out


class GmrsMarinePlan(unittest.TestCase):
    def setUp(self) -> None:
        self.profiles = (UI / "band_profile.cpp").read_text(encoding="utf-8")
        self.main = (UI / "main.cpp").read_text(encoding="utf-8")

    def profile(self, name: str) -> dict:
        match = re.search(r"\{Id::%s,.*?\},\n" % name, self.profiles, re.S)
        self.assertIsNotNone(match, "the %s profile is gone" % name)
        raw = " ".join(match.group(0).strip().rstrip(",").split())
        parts = split_fields(raw)
        self.assertEqual(len(parts), len(FIELDS),
                         "the Profile layout changed; revisit this test:\n  %r" % parts)
        out = dict(zip(FIELDS, parts))
        out["parts"] = parts
        for key in ("low_hz", "high_hz", "raster_origin_hz", "step_count", "default_step"):
            out[key] = int(out[key].lstrip("{"))
        out["steps_hz"] = [int(s) for s in re.findall(r"\d+", out["steps_hz"])]
        return out

    def assert_channels_on_raster(self, profile: dict, channels: dict, label: str) -> None:
        """Every channel on the grid the source declares, and inside the band."""
        origin = profile["raster_origin_hz"]
        step = min(profile["steps_hz"])
        off_raster = {c: hz for c, hz in channels.items() if (hz - origin) % step}
        self.assertEqual(
            off_raster, {},
            "%s: these channels are not on the %d Hz raster the source anchors "
            "at %d Hz" % (label, step, origin))
        outside = {c: hz for c, hz in channels.items()
                   if not profile["low_hz"] <= hz < profile["high_hz"]}
        self.assertEqual(outside, {},
                         "%s: these channels fall outside the profile's own "
                         "range %d..%d" % (label, profile["low_hz"], profile["high_hz"]))

    def test_gmrs_is_channelized_on_a_raster_that_fits_every_channel(self) -> None:
        gmrs = self.profile("frs_gmrs")
        self.assertEqual(gmrs["control"], "Control::channelized",
                         "free tuning can miss a 12.5 kHz channel audibly")
        self.assertIn(12_500, gmrs["steps_hz"],
                      "the 12.5 kHz step the interstitials need is gone")
        self.assert_channels_on_raster(gmrs, FRS_GMRS_CHANNELS_HZ, "FRS/GMRS")

    def test_marine_is_channelized_on_a_raster_that_fits_every_channel(self) -> None:
        marine = self.profile("marine")
        self.assertEqual(marine["control"], "Control::channelized")
        self.assertIn(25_000, marine["steps_hz"])
        self.assert_channels_on_raster(marine, MARINE_CHANNELS_HZ, "marine")

    def test_the_narrowband_window_holds_exactly_channels_8_to_14(self) -> None:
        low = int(re.search(r"kGmrsInterstitialLowHz = (\d+)", self.main).group(1))
        high = int(re.search(r"kGmrsInterstitialHighHz = (\d+)", self.main).group(1))
        for channel, hz in FRS_GMRS_CHANNELS_HZ.items():
            inside = low <= hz <= high
            self.assertEqual(inside, channel in NARROWBAND_CHANNELS,
                             "channel %d at %d Hz is on the wrong side of the "
                             "narrowband window" % (channel, hz))

    def test_the_narrowband_window_stays_inside_the_gmrs_band(self) -> None:
        """rtl_filter_default_hz checks the frequency before it checks the band."""
        gmrs = self.profile("frs_gmrs")
        low = int(re.search(r"kGmrsInterstitialLowHz = (\d+)", self.main).group(1))
        high = int(re.search(r"kGmrsInterstitialHighHz = (\d+)", self.main).group(1))
        self.assertGreaterEqual(low, gmrs["low_hz"])
        self.assertLess(high, gmrs["high_hz"],
                        "the window reaches past the top of the band, so "
                        "non-GMRS frequencies would get a 12.5 kHz filter")
        self.assertEqual(high, max(FRS_GMRS_CHANNELS_HZ[c] for c in NARROWBAND_CHANNELS),
                         "the window should end at channel 14, not beyond it")

    def test_the_narrow_filter_is_12_5_khz(self) -> None:
        width = int(re.search(r"kGmrsNarrowFilterHz = (\d+)", self.main).group(1))
        self.assertEqual(width, 12_500, "47 CFR 95.1773 sets 12.5 kHz for 8-14")
        self.assertIn("if (gmrs_channel_is_narrowband(frequency_hz)) return kGmrsNarrowFilterHz;",
                      self.main, "the default filter does not follow the channel")


if __name__ == "__main__":
    unittest.main()
