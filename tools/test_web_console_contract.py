#!/usr/bin/env python3
"""Keep zero-valued ADS-B measurements distinct from missing ones.

Zero is a legal value for every measurement the radar shows: sea level, a
stopped aircraft, due north, and level flight all read 0. Treating the value as
truthy therefore hides real data, which is how a northbound aircraft at sea
level used to lose its heading and altitude on the Companion page.

The device sends an explicit flag beside each measurement, and the page must
branch on the flag, never on the number. The same applies to position: a target
without a usable fix is listed, never plotted, because plotting it puts it at
the radar centre -- which is exactly what "no position" used to look like.
"""

from __future__ import annotations

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
UI = ROOT / "apps" / "orcsdr-tab5" / "ui"


class CompanionAdsbContract(unittest.TestCase):
    def setUp(self) -> None:
        self.backend = (UI / "web_console.cpp").read_text(encoding="utf-8")
        self.header = (UI / "web_console.hpp").read_text(encoding="utf-8")
        self.frontend = (UI / "web_console.html").read_text(encoding="utf-8")

    def test_backend_sends_a_flag_beside_every_measurement(self) -> None:
        for field in ("alt_ok", "spd_ok", "hdg_ok", "vr_ok"):
            self.assertIn('\\"%s\\":%%s' % field, self.backend,
                          "%s is not serialised" % field)

    def test_backend_sends_position_validity(self) -> None:
        self.assertIn('\\"pos\\":%s', self.backend)
        self.assertIn("relative_position_available", self.header)

    def test_position_needs_both_a_fix_and_a_receiver_location(self) -> None:
        """Range and bearing are meaningless without the receiver's own position."""
        self.assertIn("relative_position_available(true, true)", self.header)
        self.assertIn("!relative_position_available(true, false)", self.header)
        self.assertIn("!relative_position_available(false, true)", self.header)

    def test_frontend_branches_on_the_flag_not_the_value(self) -> None:
        for expression in ("a2.alt_ok?", "a2.spd_ok?", "a2.hdg_ok?", "a2.vr_ok?",
                           "t.hdg_ok"):
            self.assertIn(expression, self.frontend,
                          "the page should test %s" % expression)

    def test_frontend_never_plots_a_target_without_a_position(self) -> None:
        self.assertIn("if(!t.pos) continue;", self.frontend)

    def test_frontend_does_not_treat_zero_as_missing(self) -> None:
        for obsolete in ("a2.alt?", "a2.spd?", "a2.hdg?", "a2.vr?"):
            self.assertNotIn(obsolete, self.frontend,
                             "%s hides a legitimate zero" % obsolete)

    def test_labels_are_never_injected_as_markup(self) -> None:
        """A label arrives over the air; the rows are built from text nodes."""
        self.assertIn("document.createTextNode", self.frontend)
        self.assertNotIn("tbody')[0].innerHTML", self.frontend)

    def test_the_json_buffer_is_derived_from_the_slot_count(self) -> None:
        """A guessed buffer is how a full radar silently truncates its JSON."""
        self.assertIn("kAircraftJsonCapacity = 3 + kAircraftSlots", self.backend)


if __name__ == "__main__":
    unittest.main()
