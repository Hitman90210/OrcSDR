#include "dsp_stats.hpp"
#include <cassert>
#include <cstring>
#include <cstdio>
int main(){
  using namespace orcsdr::dsp::stats;
  char text[1024];
  add(Stage::level,100);add(Stage::level,200);add(Stage::decoders,250000);
  block_done(250100,16384,2);block_done(200,16384,0);
  format_and_reset(text,sizeof(text),1000);
  assert(std::strstr(text,"level_avg_us=150")&&std::strstr(text,"level_max_us=200"));
  assert(std::strstr(text,"decoders_max_us=250000")&&std::strstr(text,"max_us=250100"));
  assert(std::strstr(text,"queue_hwm=2"));
  format_and_reset(text,sizeof(text),1000);
  assert(std::strstr(text,"level_max_us=0")&&std::strstr(text,"decoders_max_us=0"));
  std::puts("PASS: per-stage peaks distinguish long stalls from averages and reset with the measurement window");
}
