#!/usr/bin/env python3
"""Keep button labels inside their buttons.

Written after a user reported text spilling out of the ADS-B target view's
LISTEN button, and again after the Settings data-pack row was found drawing
"UNAVAILABLE" at exactly its button's width. Both were found by eye. This finds
them by arithmetic, before they ship.

Scope is deliberately the cases that can be computed exactly. A dashboard whose
text() helper calls setTextSize(2) on the built-in 6x8 bitmap font draws every
character at exactly 12 px, so a label of N characters needs exactly 12*N px of
the button's width -- no estimate involved.

adsb_dashboard and pocsag_dashboard are excluded: their text() selects
proportional DejaVu faces at setTextSize(1), where width depends on the glyphs.
Checking those needs real font metrics, and a test that guesses would either
miss overflows or cry wolf. They are listed as unchecked rather than silently
skipped, so the gap stays visible.
"""

from __future__ import annotations

import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
UI = ROOT / "apps" / "orcsdr-tab5" / "ui"

# text() here is the built-in font at setTextSize(2): exactly 12 px per glyph.
BITMAP_SIZE2_PX = 12
BITMAP_FILES = {
    "am_dashboard.cpp", "fm_dashboard.cpp", "lora_dashboard.cpp",
    "p25_dashboard.cpp", "main.cpp", "rf_visualizer.cpp",
    "settings_app.cpp", "text_editor.cpp",
}
# Proportional DejaVu faces -- not computable without font metrics.
UNCHECKED_FILES = {"adsb_dashboard.cpp", "pocsag_dashboard.cpp"}

# button("LABEL", x, y, w, h, ...)
RE_LABEL_FIRST = re.compile(
    r'\bbutton\(\s*"((?:[^"\\]|\\.)*)"\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(\d+)\s*,\s*(\d+)')
# button(x, y, w, h, "LABEL", ...)
RE_LABEL_MID = re.compile(
    r'\bbutton\(\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*"((?:[^"\\]|\\.)*)"')
LABEL_FIRST_FILES = {"settings_app.cpp", "text_editor.cpp"}


def literal_buttons(name: str, source: str) -> list[tuple[str, int]]:
    """(label, button width) for every call with both spelled out literally."""
    flat = re.sub(r"\n\s*", " ", source)          # calls wrap across lines
    pattern = RE_LABEL_FIRST if name in LABEL_FIRST_FILES else RE_LABEL_MID
    out = []
    for match in pattern.finditer(flat):
        if name in LABEL_FIRST_FILES:
            label, _, _, width, _ = match.groups()
        else:
            _, _, width, _, label = match.groups()
        out.append((label, int(width)))
    return out


class ButtonLabelsFit(unittest.TestCase):
    def setUp(self) -> None:
        self.calls = []
        for path in sorted(UI.glob("*.cpp")):
            if path.name not in BITMAP_FILES:
                continue
            source = path.read_text(encoding="utf-8", errors="replace")
            for label, width in literal_buttons(path.name, source):
                self.calls.append((path.name, label, width))

    def test_coverage(self) -> None:
        """A regex that stops matching would pass this suite in silence."""
        self.assertGreaterEqual(
            len(self.calls), 80,
            "found only %d literal button calls; the parser has probably stopped "
            "matching after a reformat, which would make the fit test vacuous"
            % len(self.calls))

    def test_labels_fit_their_buttons(self) -> None:
        overflows = []
        for name, label, width in self.calls:
            needed = len(label) * BITMAP_SIZE2_PX
            if needed > width:
                overflows.append(
                    '%s: "%s" is %d chars = %d px in a %d px button (over by %d)'
                    % (name, label, len(label), needed, width, needed - width))
        self.assertEqual(
            overflows, [],
            "Button labels wider than their buttons. Every label here is drawn "
            "centred at 12 px per character:\n  " + "\n  ".join(overflows))

    def test_unchecked_files_are_still_proportional(self) -> None:
        """If one of these stops using DejaVu, it belongs in the exact check."""
        for name in UNCHECKED_FILES:
            source = (UI / name).read_text(encoding="utf-8", errors="replace")
            self.assertIn(
                "fonts::DejaVu", source,
                "%s no longer selects a DejaVu face, so its buttons may now be "
                "measurable exactly -- move it into BITMAP_FILES" % name)


if __name__ == "__main__":
    unittest.main()
