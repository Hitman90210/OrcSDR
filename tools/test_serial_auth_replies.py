#!/usr/bin/env python3
"""Every gated serial command must answer, not go silent.

An authenticated session ends kSessionTimeoutMs (5 s) after the last command,
so a host that pauses without sending PING is unauthenticated again without
knowing it. A handler written as

    if (strncmp(command, "RTL_TUNE ", 9) == 0 && authenticated) {

then simply does not match, the command is skipped, and the device says
nothing. From the host that is indistinguishable from a dropped command; it
cost hours of false diagnosis once. The shape that answers is

    if (strncmp(command, "RTL_TUNE ", 9) == 0) {
      if (!authenticated) { Serial.println("RTL_TUNE_ERROR auth_required"); return; }

PING and ACK are the two deliberate exceptions: hosts send them on a timer and
they are never answered, with or without a session.
"""

from __future__ import annotations

import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT / "apps" / "orcsdr-tab5" / "ui" / "main.cpp"

# Commands that may keep the silent `&& authenticated` form.
SILENT_BY_DESIGN = ('strcmp(command, "PING") == 0',
                    'strncmp(command, "ACK ", 4) == 0')


class SerialAuthReplies(unittest.TestCase):
    def setUp(self) -> None:
        self.source = MAIN.read_text(encoding="utf-8")

    def test_only_ping_and_ack_stay_silent(self) -> None:
        """A gated handler that does not reply is a command a host cannot diagnose."""
        offenders = []
        for line_number, line in enumerate(self.source.splitlines(), start=1):
            if "&& authenticated" not in line:
                continue
            if any(exempt in line for exempt in SILENT_BY_DESIGN):
                continue
            offenders.append("%s:%d: %s" % (MAIN.name, line_number, line.strip()))
        self.assertEqual(
            offenders, [],
            "These gated handlers fall through silently when the session has "
            "lapsed. Give each one an auth_required reply:\n  "
            + "\n  ".join(offenders))

    def test_the_exemptions_are_still_there(self) -> None:
        """If PING or ACK started replying, hosts would see unexpected lines."""
        for exempt in SILENT_BY_DESIGN:
            self.assertIn("%s && authenticated" % exempt, self.source,
                          "%s should keep the silent form" % exempt)

    def test_the_gated_families_reply(self) -> None:
        """Spot-check the commands a host drives most, including after a lapse."""
        for prefix in ("RTL_TUNE", "RTL_FREQ", "RTL_VOLUME", "RTL_CAPTURE",
                       "RTL_P25_SCAN", "RTL_PRESET_TUNE", "FLASH", "WIFI",
                       "WORKFLOW", "ROTATE", "PRESSURE"):
            self.assertIn('"%s_ERROR auth_required"' % prefix, self.source,
                          "%s has no auth_required reply" % prefix)

    def test_session_timeout_is_still_short_enough_to_matter(self) -> None:
        """The whole point: the window is seconds, so a lapse is routine."""
        match = re.search(r"kSessionTimeoutMs\s*=\s*(\d+)", self.source)
        self.assertIsNotNone(match, "kSessionTimeoutMs is gone; revisit this test")
        self.assertLessEqual(int(match.group(1)), 60000,
                             "a long session would change the reasoning above")


if __name__ == "__main__":
    unittest.main()
