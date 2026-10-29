#!/usr/bin/env python3
"""Run the real Tab5 Pair memory preparation against bounded allocator/reclaim stubs."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
HARNESS = r'''
#include <cassert>
#include <cstddef>
#include <cstdio>
constexpr unsigned MALLOC_CAP_INTERNAL=1,MALLOC_CAP_DMA=2,MALLOC_CAP_8BIT=4,MALLOC_CAP_RTCRAM=8;
constexpr size_t kOrcDialOnDemandDmaNeed=6144,kOrcDialRtcStackBytes=16384;
size_t dma=7379,dma_largest=7168,rtc=31744,rtc_largest=31744,recover=0;
unsigned reclaim_calls=0;
size_t heap_caps_get_free_size(unsigned caps){return (caps&MALLOC_CAP_RTCRAM)?rtc:dma;}
size_t heap_caps_get_largest_free_block(unsigned caps){return (caps&MALLOC_CAP_RTCRAM)?rtc_largest:dma_largest;}
struct SerialStub {template<class... T> void printf(const char*,T...) {}} Serial;
namespace orcsdr::memory_reclaim {
constexpr size_t kMaxHolders=8;
struct Report {};
size_t reclaim_all(Report*,size_t,size_t* count){++reclaim_calls;*count=0;dma+=recover;return recover;}
}
/* PREPARE */
int main() {
  assert(orcdial_prepare_memory() && reclaim_calls==1);
  dma=6143;recover=1024;assert(orcdial_prepare_memory() && reclaim_calls==2);
  recover=0;dma=6143;assert(!orcdial_prepare_memory());
  dma=6144;dma_largest=4095;assert(!orcdial_prepare_memory());
  dma_largest=4096;rtc=16383;assert(!orcdial_prepare_memory());
  rtc=16384;rtc_largest=12287;assert(!orcdial_prepare_memory());
  rtc_largest=12288;assert(orcdial_prepare_memory());
  assert(reclaim_calls==7);
  std::puts("ORCDIAL_MEMORY_POLICY_PASS: reclaim attempted; fresh-boot budget accepted; DMA/RTC size and fragmentation refusals enforced");
}
'''


def main():
    text = (ROOT / 'apps/orcsdr-tab5/ui/main.cpp').read_text(encoding='utf-8')
    start = text.index('bool orcdial_prepare_memory() {')
    end = text.index('\nvoid orcdial_action(', start)
    with tempfile.TemporaryDirectory(prefix='orcsdr-dial-memory-') as directory:
        path = Path(directory)
        source = path / 'policy.cpp'
        source.write_text(HARNESS.replace('/* PREPARE */', text[start:end]), encoding='utf-8')
        binary = path / 'policy'
        subprocess.run(['g++', '-std=c++17', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                        '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                        str(source), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)


if __name__ == '__main__':
    main()
