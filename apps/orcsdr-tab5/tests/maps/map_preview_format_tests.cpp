// Exercise the actual pinned M5GFX conversion path, not a reimplementation.
#include "lgfx/v1/misc/pixelcopy.hpp"
#include <array>
#include <cstdio>
#include <vector>
using namespace lgfx;
int main() {
  std::vector<uint16_t> swapped(65536),native(65536);
  for(unsigned i=0;i<65536;++i)swapped[i]=static_cast<uint16_t>(i);
  pixelcopy_t publish(swapped.data(),rgb565_nonswapped,rgb565_2Byte);
  publish.src_bitwidth=65536;
  if(publish.fp_copy(native.data(),0,65536,&publish)!=65536)return 1;
  for(unsigned i=0;i<65536;++i) {
    const uint16_t expected=static_cast<uint16_t>((i>>8)|(i<<8));
    if(native[i]!=expected)return 2;
  }
  // Forward, transposed, reversed and fractional nearest-neighbour samples.
  struct Sample {unsigned x,y;int dx,dy;unsigned count;};
  const Sample samples[]={{0,0,65536,0,256},{7,0,0,65536,256},
    {255,7,-65536,0,256},{7,255,0,-65536,256},
    {10,20,32768,0,128},{10,20,0,32768,128},
    {10,20,131072,0,64},{10,20,0,131072,64}};
  for(const auto& sample:samples) {
    std::array<uint16_t,258> a{},b{};a.fill(0xdead);b.fill(0xdead);
    pixelcopy_t old(swapped.data(),rgb565_nonswapped,rgb565_2Byte);
    pixelcopy_t next(native.data(),rgb565_nonswapped,rgb565_nonswapped);
    if(old.no_convert||!next.no_convert)return 3;
    for(auto* p:{&old,&next}) {
      p->src_bitwidth=256;p->src_x32=sample.x<<16;p->src_y32=sample.y<<16;
      p->src_x32_add=sample.dx;p->src_y32_add=sample.dy;
    }
    if(old.fp_copy(a.data(),1,sample.count+1,&old)!=sample.count+1)return 4;
    if(next.fp_copy(b.data(),1,sample.count+1,&next)!=sample.count+1)return 5;
    if(a!=b||a.front()!=0xdead||a[sample.count+1]!=0xdead)return 6;
  }
  // Native preview remains valid when the destination uses swapped RGB565.
  std::vector<uint16_t> restored(65536);
  pixelcopy_t reverse(native.data(),rgb565_2Byte,rgb565_nonswapped);
  reverse.src_bitwidth=65536;
  if(reverse.fp_copy(restored.data(),0,65536,&reverse)!=65536||restored!=swapped)return 7;
  std::puts("PREVIEW_FORMAT_PASS all 65536 values, 8 sample traversals, reverse destination");
}
