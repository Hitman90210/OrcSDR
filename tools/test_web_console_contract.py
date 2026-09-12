#!/usr/bin/env python3
"""Keep zero-valued ADS-B measurements distinct from missing measurements."""

from __future__ import annotations

import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
UI = ROOT / "apps" / "orcsdr-tab5" / "ui"


class CompanionAdsbContract(unittest.TestCase):
    def setUp(self) -> None:
        self.backend = (UI / "web_console.cpp").read_text(encoding="utf-8")
        self.frontend = (UI / "web_console.html").read_text(encoding="utf-8")

    def test_backend_serializes_measurement_validity(self) -> None:
        for field in ("alt_ok", "spd_ok", "hdg_ok", "vr_ok"):
            self.assertIn(f'\\"{field}\\":%s', self.backend)

    def test_frontend_uses_validity_not_numeric_truthiness(self) -> None:
        required = (
            "if(t.hdg_ok)",
            "if(t.alt_ok)",
            "t.spd_ok?t.spd+' kt':'—'",
            "t.vr_ok&&t.vr>64",
            "t.vr_ok&&t.vr<-64",
        )
        for expression in required:
            self.assertIn(expression, self.frontend)
        for obsolete in ("if(t.hdg){", "if(t.alt){", "(t.spd?t.spd+' kt':'—')"):
            self.assertNotIn(obsolete, self.frontend)


if __name__ == "__main__":
    unittest.main()
