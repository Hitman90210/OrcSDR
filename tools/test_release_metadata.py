#!/usr/bin/env python3
"""Keep the pinned radio firmware inputs aligned across release files."""

from __future__ import annotations

import json
import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "apps" / "orcsdr-tab5" / "main" / "idf_component.yml"
BRIDGE_MANIFEST = ROOT / "apps" / "orcsdr-c6-bridge" / "main" / "idf_component.yml"
LOCK = ROOT / "apps" / "orcsdr-tab5" / "dependencies.lock"
HOSTED = ROOT / "tools" / "release" / "hosted-c6-release.json"

# Sources that state the pinned Hosted version as a contract. Documentation and
# historical evidence are excluded on purpose; they may cite older versions.
VERSION_SCAN_SUFFIXES = {".c", ".cpp", ".h", ".hpp", ".ps1", ".py", ".yml", ".json", ".txt"}
VERSION_SCAN_ROOTS = ("apps", "components", "tools")
IGNORED_PARTS = {".git", "build", "managed_components", "site", "dist"}
# Each pattern captures a version that is being asserted as the current pin,
# so a bump to hosted-c6-release.json has to sweep every one of them.
PIN_PATTERNS = (
    r"Hosted[ -](\d+\.\d+\.\d+)",
    r"host=(\d+\.\d+\.\d+)",
    r"target=(\d+\.\d+\.\d+)",
    r"TARGET: (\d+\.\d+\.\d+)",
    r"hosted_version[\"']?\s*[:=]\s*[\"'](\d+\.\d+\.\d+)",
)


def scanned_sources() -> list[Path]:
    files: list[Path] = []
    for name in VERSION_SCAN_ROOTS:
        for path in (ROOT / name).rglob("*"):
            if not path.is_file() or path.suffix not in VERSION_SCAN_SUFFIXES:
                continue
            if any(part in IGNORED_PARTS or part.startswith("build") for part in path.parts):
                continue
            files.append(path)
    return files


def manifest_version(component: str, manifest: Path = MANIFEST) -> str:
    text = manifest.read_text(encoding="utf-8")
    match = re.search(
        rf"(?m)^  {re.escape(component)}:\s*\n(?:    .*\n)*?    version: [\"']?([^\"'\r\n]+)",
        text,
    )
    if not match:
        raise AssertionError(f"missing version for {component} in {manifest}")
    return match.group(1).strip()


def lock_version(component: str) -> str:
    text = LOCK.read_text(encoding="utf-8")
    match = re.search(
        rf"(?ms)^  {re.escape(component)}:\s*\n(.*?)(?=^  [^ \r\n].*?:\s*$|^direct_dependencies:)",
        text,
    )
    if not match:
        raise AssertionError(f"missing lock entry for {component} in {LOCK}")
    version = re.search(r"(?m)^    version: ['\"]?([^'\"\r\n]+)", match.group(1))
    if not version:
        raise AssertionError(f"missing locked version for {component}")
    return version.group(1).strip()


class ReleaseMetadataTests(unittest.TestCase):
    def test_hosted_version_is_consistent(self) -> None:
        metadata = json.loads(HOSTED.read_text(encoding="utf-8"))
        expected = metadata["hosted_version"]
        self.assertEqual(expected, manifest_version("espressif/esp_hosted"))
        self.assertEqual(expected, lock_version("espressif/esp_hosted"))

    def test_critical_dependencies_are_exactly_pinned(self) -> None:
        for component in ("m5stack/m5unified", "m5stack/m5gfx"):
            requested = manifest_version(component)
            self.assertNotRegex(requested, r"[<>=~^*]", component)
            self.assertEqual(requested.lstrip("v"), lock_version(component).lstrip("v"))

        # A release tag or a full commit SHA, never a range. The test used to
        # require a tag, which broke when upstream pinned the unreleased
        # v0.7.15 HF-routing commit directly. That is a legitimate pin -- a
        # 40-character SHA is stricter than a tag, which can be moved -- so
        # what actually matters is asserted instead: no floating operators.
        rtl_requested = manifest_version("esp_rtl_sdr")
        self.assertNotRegex(rtl_requested, r"[<>=~^*]", "esp_rtl_sdr must not float")
        self.assertRegex(rtl_requested, r"^(v\d+\.\d+\.\d+|[0-9a-f]{40})$")
        self.assertRegex(lock_version("esp_rtl_sdr"), r"^[0-9a-f]{40}$")

    def test_bridge_manifest_matches_the_pin(self) -> None:
        expected = json.loads(HOSTED.read_text(encoding="utf-8"))["hosted_version"]
        self.assertEqual(expected, manifest_version("espressif/esp_hosted", BRIDGE_MANIFEST))

    def test_no_source_states_a_stale_hosted_version(self) -> None:
        expected = json.loads(HOSTED.read_text(encoding="utf-8"))["hosted_version"]
        stale: list[str] = []
        for path in scanned_sources():
            text = path.read_text(encoding="utf-8", errors="replace")
            for lineno, line in enumerate(text.splitlines(), 1):
                for pattern in PIN_PATTERNS:
                    for found in re.findall(pattern, line):
                        if found != expected:
                            stale.append(f"{path.relative_to(ROOT)}:{lineno}: {found}")
        self.assertEqual(
            [],
            stale,
            "Sources state a Hosted version other than the pin in "
            f"tools/release/hosted-c6-release.json ({expected}):\n" + "\n".join(stale),
        )

    def test_hosted_source_revision_is_a_full_commit(self) -> None:
        metadata = json.loads(HOSTED.read_text(encoding="utf-8"))
        self.assertRegex(metadata["source_revision"], r"^[0-9a-f]{40}$")
        self.assertEqual("5.5.4", metadata["idf_version"])


if __name__ == "__main__":
    unittest.main()
