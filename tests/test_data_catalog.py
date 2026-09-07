#!/usr/bin/env python3
import json
import shutil
import subprocess
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
BUILDER = ROOT / "tools" / "data_catalog" / "build_catalog.py"


class P25CatalogTest(unittest.TestCase):
    def test_signed_p25_pack_and_destination_gate(self):
        openssl = shutil.which("openssl")
        self.assertIsNotNone(openssl)
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            profile = work / "profile.cfg"
            profile.write_text(
                "version=2\nsystem_name=Catalog Test\ncontrol_channel_hz=851012500\n",
                encoding="utf-8",
            )
            archive = work / "source.zip"
            with zipfile.ZipFile(archive, "w") as bundle:
                bundle.writestr("SOURCE.txt", "test provenance")
            key, public = work / "key.pem", work / "public.pem"
            subprocess.run(
                [openssl, "ecparam", "-name", "prime256v1", "-genkey", "-noout", "-out", key],
                check=True, capture_output=True,
            )
            subprocess.run(
                [openssl, "ec", "-in", key, "-pubout", "-out", public],
                check=True, capture_output=True,
            )
            spec = {
                "schema": "catalog-input-v1",
                "generated_at": "2026-09-06",
                "minimum_firmware": "0.2.0",
                "packs": [{
                    "id": "p25_catalog_test",
                    "title": "CATALOG TEST",
                    "version": "2026-09-06",
                    "source_date": "2026-09-06",
                    "source_url": "https://example.invalid/source",
                    "redistribution": "test only",
                    "runtime": str(profile),
                    "archive": str(archive),
                    "runtime_destination": "/orcsdr/p25/p25_catalog_test/profile.cfg",
                    "archive_destination": "/orcsdr/data/p25_catalog_test_source.zip",
                }],
            }
            source = work / "input.json"
            source.write_text(json.dumps(spec), encoding="utf-8")
            command = [
                sys.executable, str(BUILDER), str(source), "--out", str(work / "out"),
                "--private-key", str(key), "--verify-public-key", str(public),
                "--release-base", "https://example.invalid/release", "--openssl", openssl,
            ]
            subprocess.run(command, check=True, capture_output=True, text=True)
            catalog = json.loads((work / "out" / "catalog-v1.json").read_text(encoding="utf-8"))
            self.assertEqual(catalog["packs"][0]["title"], "CATALOG TEST")

            profile.write_text(
                "system_name=version=2\nnote=control_channel_hz=851012500\n",
                encoding="utf-8",
            )
            rejected = subprocess.run(command, capture_output=True, text=True)
            self.assertNotEqual(rejected.returncode, 0)

            profile.write_text(
                "version=2\nsystem_name=Catalog Test\ncontrol_channel_hz=851012500\n",
                encoding="utf-8",
            )
            spec["packs"][0]["runtime_destination"] = "/orcsdr/p25/wrong/profile.cfg"
            source.write_text(json.dumps(spec), encoding="utf-8")
            rejected = subprocess.run(command, capture_output=True, text=True)
            self.assertNotEqual(rejected.returncode, 0)


if __name__ == "__main__":
    unittest.main()
