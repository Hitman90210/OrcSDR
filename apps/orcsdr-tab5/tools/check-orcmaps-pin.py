"""Reject a stale locked OrcMaps revision even if component resolution succeeds."""
from pathlib import Path
import sys
import yaml


def check(root: Path) -> None:
    requested = yaml.safe_load((root / "main/idf_component.yml").read_text())["dependencies"]["orcmaps"]
    locked = yaml.safe_load((root / "dependencies.lock").read_text())["dependencies"]["orcmaps"]
    if requested["version"] != locked["version"] or requested["git"] != locked["source"]["git"]:
        raise ValueError(f"OrcMaps pin mismatch: requested {requested['version']}, locked {locked['version']}")
    fetched = (root / "managed_components/orcmaps/.component_hash").read_text().strip()
    if fetched != locked["component_hash"]:
        raise ValueError("Fetched OrcMaps component hash differs from dependencies.lock")
    print(f"PASS: requested, locked and fetched OrcMaps agree: {locked['version']}")


if __name__ == "__main__":
    try:
        check(Path(sys.argv[1]))
    except (OSError, ValueError, KeyError, yaml.YAMLError) as error:
        sys.exit(str(error))
