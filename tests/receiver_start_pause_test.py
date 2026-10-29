"""Exercise both production request-consumption guards while I/O is paused."""
from pathlib import Path
import re
import shutil
import subprocess

root = Path(__file__).resolve().parents[1]
source = (root / "apps/orcsdr-tab5/ui/main.cpp").read_text(encoding="utf-8")
guards = []
for prefix in ("g_rtl != nullptr", "rtl_sdr_device != nullptr"):
    matches = re.findall(
        r"if \((" + re.escape(prefix) + r"[^{};]*?rtl_capture_requested\.exchange\(false, std::memory_order_acq_rel\))\) \{",
        source,
    )
    assert len(matches) == 1, (prefix, len(matches))
    guards.append(matches[0])

harness = r'''
#include <atomic>
#include <cassert>
#include <cstdio>
#include <initializer_list>
void* g_rtl=reinterpret_cast<void*>(1);
void* rtl_sdr_device=reinterpret_cast<void*>(1);
bool rtl_sdr_gone=false,paused=false;
std::atomic<bool> g_rtl_device_ready{true},rtl_capture_requested{false};
bool radio_io_paused(){return paused;}
'''
for index, guard in enumerate(guards):
    harness += f"bool consume_{index}() {{return {guard};}}\n"
harness += r'''
int main(){
 for(auto consume:{consume_0,consume_1}) {
  rtl_capture_requested=true;paused=true;
  for(int i=0;i<5;++i)assert(!consume()&&rtl_capture_requested);
  paused=false;assert(consume()&&!rtl_capture_requested);
  assert(!consume());
  rtl_capture_requested=true;g_rtl_device_ready=false;rtl_sdr_gone=true;
  assert(!consume()&&rtl_capture_requested);
  g_rtl_device_ready=true;rtl_sdr_gone=false;assert(consume());
 }
 std::puts("PASS: native and legacy start requests remain pending under I/O pause and consume once after resume");
}
'''
folder = root / ".local/maps/receiver-start-pause"
folder.mkdir(parents=True, exist_ok=True)
cpp, exe = folder / "test.cpp", folder / "test"
cpp.write_text(harness, encoding="utf-8")


def linux(path):
    return "/mnt/" + str(path)[0].lower() + str(path)[2:].replace("\\", "/")


if shutil.which("g++"):
    command = ["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror", str(cpp), "-o", str(exe)]
    run = [str(exe)]
else:
    command = ["wsl", "g++", "-std=c++17", "-Wall", "-Wextra", "-Werror", linux(cpp), "-o", linux(exe)]
    run = ["wsl", linux(exe)]
subprocess.run(command, check=True)
subprocess.run(run, check=True)
