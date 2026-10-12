#!/usr/bin/env python3
"""Every serial command must get an answer, or be silent by deliberate design.

An authenticated session ends kSessionTimeoutMs (5 s) after the last command, so
a host that pauses without sending PING is unauthenticated again without knowing
it. A handler written as

    if (strncmp(command, "RTL_TUNE ", 9) == 0 && authenticated) {

then simply does not match, the command is skipped, and the device says nothing.
From the host that is indistinguishable from a dropped command, and it cost one
confident, wrong diagnosis. The shape that answers is

    if (strncmp(command, "RTL_TUNE ", 9) == 0) {
      if (!authenticated) { Serial.println("RTL_TUNE_ERROR auth_required"); return; }

An unrecognised line is the other half of the same ambiguity, so process_command
ends with a CMD_ERROR reply. That is only safe while every handling branch
returns, which is why PING and ACK claim their command and return silently
rather than being left to fall through.
"""

from __future__ import annotations

import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT / "apps" / "orcsdr-tab5" / "ui" / "main.cpp"


class SerialAuthReplies(unittest.TestCase):
    def setUp(self) -> None:
        self.source = MAIN.read_text(encoding="utf-8")

    def test_no_handler_is_gated_silently(self) -> None:
        """`&& authenticated` in a match condition means a silent skip."""
        offenders = [
            "%s:%d: %s" % (MAIN.name, number, line.strip())
            for number, line in enumerate(self.source.splitlines(), start=1)
            if "&& authenticated" in line
        ]
        self.assertEqual(
            offenders, [],
            "These handlers skip the command silently once the session has "
            "lapsed. Match first, then check the session inside the branch:\n  "
            + "\n  ".join(offenders))

    def test_ping_and_ack_claim_their_command_and_stay_silent(self) -> None:
        """Both are sent on a timer and must never be answered, session or not."""
        self.assertIn('if (strcmp(command, "PING") == 0) {', self.source)
        self.assertIn("if (authenticated) last_ping_ms = millis();", self.source)
        self.assertIn('if (strncmp(command, "ACK ", 4) == 0) {', self.source)

    def test_gated_families_reply(self) -> None:
        for prefix in ("RTL_TUNE", "RTL_FREQ", "RTL_VOLUME", "RTL_CAPTURE",
                       "RTL_P25_SCAN", "RTL_PRESET_TUNE", "RTL_LORA_TUNE",
                       "FLASH", "WIFI", "WORKFLOW", "ROTATE", "PRESSURE"):
            self.assertIn('"%s_ERROR auth_required"' % prefix, self.source,
                          "%s has no auth_required reply" % prefix)

    def test_unknown_commands_are_answered_without_echoing_arguments(self) -> None:
        self.assertIn('CMD_ERROR unknown_command verb=%s', self.source,
                      "an unrecognised command should say so")
        self.assertIn("if (command[0] == '{') return;", self.source,
                      "JSON frames are not console commands and must stay unanswered")
        # The reply must carry the verb only: arguments hold key and password hex.
        self.assertNotIn('CMD_ERROR unknown_command %s\\n", command', self.source,
                         "never echo the whole command line")

    def test_session_timeout_is_still_short_enough_to_matter(self) -> None:
        match = re.search(r"kSessionTimeoutMs\s*=\s*(\d+)", self.source)
        self.assertIsNotNone(match, "kSessionTimeoutMs is gone; revisit this test")
        self.assertLessEqual(int(match.group(1)), 60000,
                             "a long session would change the reasoning above")


if __name__ == "__main__":
    unittest.main()
