"""Host regression test of the installed SDIO allocation function; run after IDF configure."""
from pathlib import Path
import argparse
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--cc", default=os.environ.get("CC", "gcc"))
args = parser.parse_args()
source = root / "apps/orcsdr-tab5/managed_components/espressif__esp_hosted/host/mcu/eh_host_mcu_transport/src/eh_host_bus_sdio.c"
if not source.exists():
    parser.error("Native IDF configure must install esp_hosted before running this test")
text = source.read_text(encoding="utf-8")
start = text.index("static inline void *sdio_buffer_alloc(")
brace = text.index("{", start)
depth, end = 1, brace + 1
while depth:
    depth += (text[end] == "{") - (text[end] == "}")
    end += 1
with tempfile.TemporaryDirectory(prefix="orcsdr-sdio-dma-") as temporary:
    directory = Path(temporary)
    (directory / "sdio_buffer_alloc.inc").write_text(text[start:end], encoding="utf-8")
    output = directory / "test"
    subprocess.run([args.cc, "-std=c11", "-Wall", "-Wextra", "-Werror",
                    "-Wno-unused-function", "-fsanitize=address,undefined",
                    "-fno-sanitize-recover=all", "-I", str(directory),
                    str(root / "tests/hosted_sdio_dma_tests.c"), "-o", str(output)], check=True)
    subprocess.run([str(output)], check=True)
print("HOSTED_SDIO_DMA_TEST_PASS")
