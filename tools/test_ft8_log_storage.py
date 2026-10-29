#!/usr/bin/env python3
"""Fault-inject the actual FT8 log flush function, without a device or SD writes."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
HARNESS = r'''
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <cstdlib>
struct Faults {
  bool mount=true, open=true, header=true, write=true, sync=true, close=true, exists=false;
  size_t dma=8192; unsigned opens=0, closes=0, syncs=0, headers=0;
} faults;
namespace orcsdr::storage {enum class EntryStatus {missing,file,directory,error};}
struct File {
  explicit operator bool() const { return faults.open; }
  size_t size() const { return faults.exists ? 128 : 0; }
  bool seek(size_t) { return true; }
  size_t print(const char* text) { ++faults.headers; return faults.header ? std::strlen(text) : 0; }
  size_t write(const uint8_t*,size_t n) { return faults.write ? n : n-1; }
  bool sync() { ++faults.syncs; return faults.sync; }
  bool close() { ++faults.closes; return faults.close; }
};
struct FileSystem {
  bool mkdir(const char*) { return true; }
  bool exists(const char*) { return faults.exists; }
  orcsdr::storage::EntryStatus status(const char*) {
    return faults.exists ? orcsdr::storage::EntryStatus::file : orcsdr::storage::EntryStatus::missing;
  }
  File open(const char*,const char*,bool) { ++faults.opens; return {}; }
} filesystem;
FileSystem* g_sd_fs=&filesystem;
struct SerialStub {
  void println(const char*) {}
  template<class... T> void printf(const char*,T...) {}
} Serial;
constexpr unsigned MALLOC_CAP_INTERNAL=1,MALLOC_CAP_DMA=2;
constexpr size_t kFt8LogMinDmaBlock=4096,kFt8LogMaxFileBytes=4194304;
constexpr const char* FILE_APPEND="a";
size_t heap_caps_get_largest_free_block(unsigned) { return faults.dma; }
bool ensure_tab5_sd() { return faults.mount; }
uint32_t millis() { return 30000; }
uint32_t g_ft8_log_last_try_ms=0,g_ft8_log_stage_rows=3,g_ft8_log_stage_day=1;
uint32_t g_ft8_log_rows=0,g_ft8_log_dropped=0,g_ft8_log_errors=0;
char staging[]="test\n";
char* g_ft8_log_stage=staging;
size_t g_ft8_log_stage_len=5;
/* FLUSH */
void reset() {
  faults={}; g_ft8_log_stage_rows=3; g_ft8_log_stage_len=5;
  g_ft8_log_rows=0; g_ft8_log_dropped=0; g_ft8_log_errors=0;
  g_ft8_log_last_try_ms=0;
}
void retained() {
  assert(!ft8_log_flush(true));
  assert(g_ft8_log_stage_rows==3 && g_ft8_log_stage_len==5);
  assert(g_ft8_log_rows==0 && g_ft8_log_errors==1);
}
int main(int argc,char** argv) {
  assert(argc==2);reset();
  switch(std::atoi(argv[1])) {
    case 0:faults.close=false;retained();assert(faults.closes==1);break;
    case 1:faults.sync=false;retained();assert(faults.closes==1);break;
    case 2:faults.header=false;retained();assert(faults.closes==1);break;
    case 3:faults.write=false;retained();assert(faults.closes==1);break;
    case 4:faults.open=false;retained();assert(faults.closes==0);break;
    case 5:faults.mount=false;retained();assert(faults.opens==0);break;
    case 6:faults.dma=4095;assert(!ft8_log_flush(true)&&faults.opens==0&&g_ft8_log_stage_rows==3);break;
    case 7:
      assert(ft8_log_flush(true));
      assert(g_ft8_log_rows==3&&g_ft8_log_stage_rows==0&&g_ft8_log_stage_len==0);
      assert(faults.syncs==1&&faults.closes==1&&g_ft8_log_errors==0);
      assert(ft8_log_flush(true)&&faults.opens==1);break;
    case 8:
      faults.exists=true;faults.header=false;
      assert(ft8_log_flush(true)&&faults.headers==0&&g_ft8_log_rows==3);break;
    default:assert(false);
  }
  std::puts("FT8_LOG_STORAGE_PASS: close/sync/header/write/open/mount failures retain staged rows; low DMA defers; success commits");
}
'''


def main():
    text = (ROOT / 'apps/orcsdr-tab5/ui/main.cpp').read_text(encoding='utf-8')
    start = text.index('static bool ft8_log_flush(bool force) {')
    end = text.index('\n// ---- Stations heard', start)
    source = HARNESS.replace('/* FLUSH */', text[start:end])
    with tempfile.TemporaryDirectory(prefix='orcsdr-ft8-log-') as directory:
        path = Path(directory)
        (path / 'flush.cpp').write_text(source, encoding='utf-8')
        executable = path / 'flush'
        subprocess.run(['g++', '-std=c++17', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                        '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                        str(path / 'flush.cpp'), '-o', str(executable)], check=True)
        # Independent failure cases need fresh static retry state, just as a
        # fresh process does; sequential retry behavior has its own byte test.
        for scenario in range(9):
            subprocess.run([str(executable), str(scenario)], check=True)


if __name__ == '__main__':
    main()
