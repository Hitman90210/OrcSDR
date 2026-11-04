#!/usr/bin/env python3
"""Refuse a commit that would publish a local secret.

This repository is public, so a value that identifies the operator's network -
the Wi-Fi SSID above all - must never reach a commit. .gitignore only protects
files that happen to sit in an ignored folder; it does nothing about a value
pasted into source, which is how an IP reached a static_assert in c0200ecd.

The values themselves are deliberately NOT in this file. They live in
.local/secret-patterns.txt, which .gitignore excludes, so this script stays
safe to commit and review while the secrets stay on the machine that has them.

Pattern file format, one per line, blank lines and # comments ignored:

    LABEL = VALUE          literal, matched case-insensitively
    LABEL = re:REGEX       Python regular expression

A match blocks the commit. Nothing that matched is ever printed: the report
names the label and the location, and shows the value only as a length and its
first character, so this hook's own output can be pasted into an issue safely.
"""
from __future__ import annotations

import os
import re
import subprocess
import sys
from pathlib import Path

DEFAULT_PATTERN_FILE = Path(".local/secret-patterns.txt")


def repo_root() -> Path:
    out = subprocess.run(["git", "rev-parse", "--show-toplevel"],
                         capture_output=True, text=True, check=True).stdout.strip()
    return Path(out)


def load_patterns(path: Path) -> list[tuple[str, re.Pattern[str], str]]:
    """-> [(label, compiled, raw_value)]"""
    patterns: list[tuple[str, re.Pattern[str], str]] = []
    for lineno, raw in enumerate(path.read_text("utf-8", errors="replace").splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        label, _, value = line.partition("=")
        label, value = label.strip(), value.strip()
        if not value:                      # a bare value with no label
            label, value = f"line {lineno}", line
        try:
            if value.startswith("re:"):
                compiled = re.compile(value[3:], re.IGNORECASE)
            else:
                compiled = re.compile(re.escape(value), re.IGNORECASE)
        except re.error as exc:
            print(f"  secret-patterns.txt line {lineno}: bad regex ({exc}) - skipped",
                  file=sys.stderr)
            continue
        patterns.append((label, compiled, value))
    return patterns


def mask(value: str) -> str:
    if value.startswith("re:"):
        return "<regex>"
    if len(value) <= 2:
        return "*" * len(value)
    return f"{value[0]}{'*' * (len(value) - 1)} ({len(value)} chars)"


def staged_files() -> list[str]:
    out = subprocess.run(["git", "diff", "--cached", "--name-only", "--diff-filter=ACMR"],
                         capture_output=True, text=True).stdout
    return [f for f in out.splitlines() if f]


def added_lines() -> list[tuple[str, int, str]]:
    """Staged added lines as (path, line number in the new file, text)."""
    out = subprocess.run(["git", "diff", "--cached", "--no-color", "--unified=0",
                          "--diff-filter=ACMR"],
                         capture_output=True, text=True).stdout
    results: list[tuple[str, int, str]] = []
    path, lineno = "?", 0
    for line in out.splitlines():
        if line.startswith("+++ b/"):
            path, lineno = line[6:], 0
        elif line.startswith("@@"):
            match = re.search(r"\+(\d+)", line)
            lineno = int(match.group(1)) if match else 0
        elif line.startswith("+") and not line.startswith("+++"):
            results.append((path, lineno, line[1:]))
            lineno += 1
    return results


def main() -> int:
    root = repo_root()
    os.chdir(root)
    pattern_file = Path(os.environ.get("ORCSDR_SECRET_PATTERNS", DEFAULT_PATTERN_FILE))
    if not pattern_file.is_file():
        print(f"warning: {pattern_file} not found, so nothing is being screened.",
              file=sys.stderr)
        print("         Copy tools/hooks/secret-patterns.example.txt there to enable it.",
              file=sys.stderr)
        return 0

    patterns = load_patterns(pattern_file)
    if not patterns:
        print(f"warning: {pattern_file} defines no patterns.", file=sys.stderr)
        return 0

    hits: list[str] = []
    for path, lineno, text in added_lines():
        for label, compiled, value in patterns:
            if compiled.search(text):
                hits.append(f"  {path}:{lineno}  matches {label} = {mask(value)}")
    for path in staged_files():
        for label, compiled, value in patterns:
            if compiled.search(path):
                hits.append(f"  {path}  (file name) matches {label} = {mask(value)}")

    if not hits:
        return 0

    print("", file=sys.stderr)
    print("COMMIT REFUSED: staged changes contain a value that must not be published.",
          file=sys.stderr)
    print("", file=sys.stderr)
    for hit in dict.fromkeys(hits):
        print(hit, file=sys.stderr)
    print("", file=sys.stderr)
    print("This repository is public. Remove the value, or replace it with an example", file=sys.stderr)
    print("from a documentation range (198.51.100.0/24, example.com).", file=sys.stderr)
    print("If it is genuinely safe:  git commit --no-verify", file=sys.stderr)
    print("", file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())
