#!/usr/bin/env python3
"""Keep tracked sources readable as text.

Written after `same_decoder.cpp` was found holding two raw 0x00 bytes inside
char literals where the two-character escape `'\\0'` belonged. It compiled to
the right value, so nothing misbehaved at runtime -- but a NUL byte makes the
whole file binary to grep, diff and every review tool, which is how it survived
several passes unnoticed. GCC's "null character(s) preserved in literal" was
the only thing that ever said so, buried in a build log.

The cause is worth naming because it recurs: a shell heredoc eats backslashes,
so a script that means to write `'\\0'` writes a literal NUL instead. Any
generated edit can do this, and it is silent.

Both checks here are about whether a file can be read, not about style.
"""

from __future__ import annotations

import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCAN_ROOTS = ("apps", "components", "tools", "docs", ".github")
# Text formats where a NUL or a BOM is a defect rather than content. Binary
# assets -- images, firmware, packed fonts -- are excluded by omission.
TEXT_SUFFIXES = {
    ".c", ".cpp", ".h", ".hpp",
    ".py", ".ps1", ".sh",
    ".md", ".yml", ".yaml", ".json", ".txt", ".html", ".css", ".csv",
    ".cmake", ".lock",
}
IGNORED_PARTS = {".git", "managed_components", "site", "dist", "node_modules",
                 "__pycache__", ".local", ".venv"}


def tracked_text_files() -> list[Path]:
    files: list[Path] = []
    for name in SCAN_ROOTS:
        root = ROOT / name
        if not root.exists():
            continue
        for path in root.rglob("*"):
            if not path.is_file() or path.suffix not in TEXT_SUFFIXES:
                continue
            if any(part in IGNORED_PARTS or part.startswith("build")
                   for part in path.parts):
                continue
            files.append(path)
    return sorted(files)


class SourceHygiene(unittest.TestCase):
    def setUp(self) -> None:
        self.files = tracked_text_files()
        # A scan that silently matches nothing would pass forever.
        self.assertGreater(len(self.files), 100,
                           "source scan found too few files; check SCAN_ROOTS")

    def test_no_nul_bytes(self) -> None:
        """A NUL byte turns a source file binary to every text tool."""
        offenders = []
        for path in self.files:
            data = path.read_bytes()
            count = data.count(b"\x00")
            if count:
                offset = data.index(b"\x00")
                line = data[:offset].count(b"\n") + 1
                offenders.append(
                    f"{path.relative_to(ROOT)}: {count} NUL byte(s), first at line {line}"
                )
        self.assertEqual(
            offenders, [],
            "Raw NUL bytes in text sources. In a char literal this is almost "
            "always a mangled '\\0' escape -- write the file with a real editor "
            "or a Python script, not a shell heredoc:\n  " + "\n  ".join(offenders))

    def test_no_utf8_bom(self) -> None:
        """A BOM leaks into compiler output, commit subjects and diffs."""
        offenders = [
            str(path.relative_to(ROOT))
            for path in self.files
            if path.read_bytes()[:3] == b"\xef\xbb\xbf"
        ]
        self.assertEqual(
            offenders, [],
            "UTF-8 BOM in text sources. PowerShell's Set-Content and Out-File "
            "add one unless told otherwise:\n  " + "\n  ".join(offenders))

    def test_decodes_as_utf8(self) -> None:
        """Anything that is not valid UTF-8 will render as mojibake somewhere."""
        offenders = []
        for path in self.files:
            try:
                path.read_bytes().decode("utf-8")
            except UnicodeDecodeError as exc:
                offenders.append(f"{path.relative_to(ROOT)}: {exc}")
        self.assertEqual(offenders, [],
                         "Text sources that are not valid UTF-8:\n  "
                         + "\n  ".join(offenders))


if __name__ == "__main__":
    unittest.main()
