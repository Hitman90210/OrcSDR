#!/usr/bin/env python3
"""Apply OrcSDR's tracked compatibility fixes to IDF-managed M5 components."""

from __future__ import annotations

import subprocess
from pathlib import Path


APP = Path(__file__).resolve().parents[1]
ROOT = APP.parents[1]
PATCH = APP / "tools" / "patches" / "m5gfx-tab5-pageflip.patch"
REGISTRATION = """idf_component_register(
    SRCS ${SRCS}
    INCLUDE_DIRS ${COMPONENT_ADD_INCLUDEDIRS}
    REQUIRES ${COMPONENT_REQUIRES}
    )"""


def update_registration(path: Path) -> None:
    if not path.is_file():
        raise SystemExit(f"managed component is unavailable: {path}")
    original = path.read_text(encoding="utf-8")
    updated = original.replace("register_component()", REGISTRATION)
    if updated != original:
        path.write_text(updated, encoding="utf-8", newline="\n")
        print(f"updated legacy component registration: {path.relative_to(APP)}")


def git_apply(*args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        ["git", "-C", str(ROOT), "apply", *args, "--directory=apps/orcsdr-tab5", str(PATCH)],
        text=True,
        capture_output=True,
    )


def main() -> None:
    for name in ("m5stack__m5gfx", "m5stack__m5unified"):
        update_registration(APP / "managed_components" / name / "CMakeLists.txt")

    if git_apply("--reverse", "--check", "--ignore-space-change").returncode == 0:
        print("M5GFX Tab5 page-flip patch already applied")
        return
    check = git_apply("--check", "--ignore-space-change")
    if check.returncode != 0:
        raise SystemExit(check.stderr or "managed M5GFX does not match the tracked patch")
    apply = git_apply("--ignore-space-change")
    if apply.returncode != 0:
        raise SystemExit(apply.stderr or "unable to apply M5GFX Tab5 page-flip patch")
    print("applied M5GFX Tab5 page-flip patch")


if __name__ == "__main__":
    main()
