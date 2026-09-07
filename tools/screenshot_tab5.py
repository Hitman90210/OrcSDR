#!/usr/bin/env python3
"""Grab a quick, exact screenshot from a running OrcSDR Tab5 for bug reports.

Reuses the firmware's authenticated UI_CAPTURE/UI_DOC_SHOW serial protocol
(the same one the official help-media pipeline uses) instead of building a
new one. Screens are limited to the firmware's documented screen catalog
(UI_DOC_LIST) -- there's no way to freeform-capture arbitrary touch
navigation, only the named dashboards/settings/overlay views.

Examples:
    python tools/screenshot_tab5.py --list
    python tools/screenshot_tab5.py fm.listen
    python tools/screenshot_tab5.py settings.location-adsb --mode demo
    python tools/screenshot_tab5.py p25.monitor adsb.radar --port COM3

Requires the SD card to be mounted on the Tab5 (UI_CAPTURE writes the BMP to
/orcsdr/screenshots/ before this tool retrieves it over serial).
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from help_media import Tab5  # noqa: E402


def main() -> None:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("screens", nargs="*",
                         help="Screen IDs to capture, e.g. fm.listen p25.monitor")
    parser.add_argument("--port", default="COM3")
    parser.add_argument("--mode", choices=("live", "demo"), default="live",
                         help="Falls back to whichever mode the screen actually supports.")
    parser.add_argument("--out", type=Path, default=ROOT / "artifacts" / "screenshots")
    parser.add_argument("--pairing-key", type=Path, default=ROOT / ".orclink" / "ui-doc.key")
    parser.add_argument("--list", action="store_true",
                         help="List every capturable screen ID and exit.")
    args = parser.parse_args()

    print(f"Connecting to {args.port}...")
    client = Tab5(args.port, args.pairing_key)
    try:
        client.authenticate()
        firmware, screens = client.doc_list()

        if args.list or not args.screens:
            print(f"firmware: {firmware}")
            for screen_id in sorted(screens):
                print(f"  {screen_id}  (modes: {','.join(sorted(screens[screen_id]))})")
            if not args.screens:
                print("\nPass one or more screen IDs to capture, e.g.:")
                print("  python tools/screenshot_tab5.py fm.listen")
            return

        args.out.mkdir(parents=True, exist_ok=True)
        for screen_id in args.screens:
            if screen_id not in screens:
                print(f"unknown screen id: {screen_id} (--list to see valid ones)",
                      file=sys.stderr)
                continue
            available = screens[screen_id]
            mode = args.mode if args.mode in available else sorted(available)[0]
            if mode != args.mode:
                print(f"{screen_id}: {args.mode} unavailable, using {mode}", file=sys.stderr)

            client.send(f"UI_DOC_SHOW {screen_id} {mode}")
            shown = client.wait(("UI_DOC_SHOW_DONE", "UI_DOC_ERROR"))
            if shown.startswith("UI_DOC_ERROR"):
                print(f"{screen_id}: {shown}", file=sys.stderr)
                continue

            slug = screen_id.replace(".", "-")
            client.send(f"UI_CAPTURE {slug}")
            result = client.wait(("UI_CAPTURE_DONE", "UI_CAPTURE_ERROR"), 30)
            if result.startswith("UI_CAPTURE_ERROR"):
                print(f"{screen_id}: {result}", file=sys.stderr)
                continue

            destination = args.out / f"{slug}.bmp"
            client.get_file(f"/orcsdr/screenshots/{slug}.bmp", destination)
            print(f"saved {destination}")
    finally:
        try:
            client.send("UI_DOC_EXIT")
            # The device dumps a backlog of unrelated m5tab5.serial telemetry
            # right after authentication; give the reply plenty of room to
            # arrive behind it rather than reporting a false failure.
            client.wait(("UI_DOC_EXIT_DONE", "UI_DOC_ERROR"), 25)
        except Exception as error:  # noqa: BLE001 - best-effort restore on the way out
            print(f"warning: UI_DOC_EXIT failed: {error}", file=sys.stderr)
        client.close()


if __name__ == "__main__":
    main()
