#!/usr/bin/env python3
"""The FRS/GMRS and marine channel plans, checked against the band table.

Two separate claims are tested here, because the comment in band_profile.cpp
asserts the first and the firmware depends on the second.

1. A 12.5 kHz raster anchored at 462.5500 MHz lands exactly on all 22 FRS/GMRS
   channels, including the 467 MHz interstitials. That is what makes
   Control::channelized correct for the whole band rather than only its lower
   half, and it is arithmetic, so it is checked by arithmetic.

2. Channels 8-14 are the 467 MHz interstitials, limited to 12.5 kHz by
   47 CFR 95.1773 while the rest of the plan runs wider. The receiver's default
   filter has to follow the channel, or the neighbouring one bleeds in.
"""

from __future__ import annotations

import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
UI = ROOT / "apps" / "orcsdr-tab5" / "ui"

GMRS_ORIGIN_HZ = 462_550_000
GMRS_RASTER_HZ = 12_500

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


class GmrsMarinePlan(unittest.TestCase):
    def setUp(self) -> None:
        self.profiles = (UI / "band_profile.cpp").read_text(encoding="utf-8")
        self.main = (UI / "main.cpp").read_text(encoding="utf-8")

    def test_every_gmrs_channel_sits_on_the_raster(self) -> None:
        """The claim that justifies one channelized profile for both segments."""
        off_raster = {
            channel: hz for channel, hz in FRS_GMRS_CHANNELS_HZ.items()
            if (hz - GMRS_ORIGIN_HZ) % GMRS_RASTER_HZ
        }
        self.assertEqual(off_raster, {},
                         "these channels are not on a 12.5 kHz raster from 462.5500 MHz")

    def test_gmrs_is_channelized_on_that_raster(self) -> None:
        entry = re.search(r"\{Id::frs_gmrs,.*?\},", self.profiles, re.S)
        self.assertIsNotNone(entry, "the FRS/GMRS profile is gone")
        text = entry.group(0)
        self.assertIn("Control::channelized", text,
                      "free tuning can miss a 12.5 kHz channel audibly")
        self.assertIn(str(GMRS_ORIGIN_HZ), text, "the raster origin moved")

    def test_marine_is_channelized_on_25_khz(self) -> None:
        entry = re.search(r"\{Id::marine,.*?\},", self.profiles, re.S)
        self.assertIsNotNone(entry, "the marine profile is gone")
        self.assertIn("Control::channelized", entry.group(0))
        self.assertIn("25000", entry.group(0))
        for channel, hz in MARINE_CHANNELS_HZ.items():
            self.assertEqual((hz - 156_000_000) % 25_000, 0,
                             "marine channel %d is off the 25 kHz raster" % channel)

    def test_the_narrowband_window_holds_exactly_channels_8_to_14(self) -> None:
        low = int(re.search(r"kGmrsInterstitialLowHz = (\d+)", self.main).group(1))
        high = int(re.search(r"kGmrsInterstitialHighHz = (\d+)", self.main).group(1))
        for channel, hz in FRS_GMRS_CHANNELS_HZ.items():
            inside = low <= hz <= high
            self.assertEqual(inside, channel in NARROWBAND_CHANNELS,
                             "channel %d at %d Hz is on the wrong side of the "
                             "narrowband window" % (channel, hz))

    def test_the_narrow_filter_is_12_5_khz(self) -> None:
        width = int(re.search(r"kGmrsNarrowFilterHz = (\d+)", self.main).group(1))
        self.assertEqual(width, 12_500, "47 CFR 95.1773 sets 12.5 kHz for 8-14")
        self.assertIn("if (gmrs_channel_is_narrowband(frequency_hz)) return kGmrsNarrowFilterHz;",
                      self.main, "the default filter does not follow the channel")


if __name__ == "__main__":
    unittest.main()
