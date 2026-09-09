#!/usr/bin/env python3
"""Apply OrcSDR's tracked compatibility fixes to IDF-managed components."""

from __future__ import annotations

import subprocess
from pathlib import Path


APP = Path(__file__).resolve().parents[1]
ROOT = APP.parents[1]
PATCHES = (
    ("M5GFX Tab5 page-flip", APP / "tools" / "patches" / "m5gfx-tab5-pageflip.patch"),
    (
        "ESP-Hosted task lifecycle",
        APP / "tools" / "patches" / "esp-hosted-task-lifecycle.patch",
    ),
)
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


def git_apply(patch: Path, *args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        ["git", "-C", str(ROOT), "apply", *args, "--directory=apps/orcsdr-tab5", str(patch)],
        text=True,
        capture_output=True,
    )


def main() -> None:
    for name in ("m5stack__m5gfx", "m5stack__m5unified"):
        update_registration(APP / "managed_components" / name / "CMakeLists.txt")

    for label, patch in PATCHES:
        if git_apply(patch, "--reverse", "--check", "--ignore-space-change").returncode == 0:
            print(f"{label} patch already applied")
            continue
        check = git_apply(patch, "--check", "--ignore-space-change")
        if check.returncode != 0:
            raise SystemExit(check.stderr or f"managed component does not match {label} patch")
        apply = git_apply(patch, "--ignore-space-change")
        if apply.returncode != 0:
            raise SystemExit(apply.stderr or f"unable to apply {label} patch")
        print(f"applied {label} patch")


if __name__ == "__main__":
    main()
