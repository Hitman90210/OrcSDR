#include "adsb_dashboard.hpp"
#include <cassert>
#include <cstdio>
#include <initializer_list>
int main() {
  using namespace orcsdr::adsb;
  uint16_t value=25;
  for(const char* text:{"", "0", "-10", "+10", "11", "65546", "4294967306",
      "100 extra", "10.0", "1e1", "10 ", " 10"}) {
    assert(!parse_map_range(text,&value)&&value==25);
  }
  assert(!parse_map_range(nullptr,&value)&&!parse_map_range("10",nullptr));
  assert(!valid_map_range(-65526)&&!valid_map_range(65546));
  assert(parse_map_range("10",&value)&&value==10);
  assert(parse_map_range("25",&value)&&value==25);
  assert(parse_map_range("50",&value)&&value==50);
  assert(parse_map_range("100",&value)&&value==100);
  for(const auto range:kMapRangesNm)assert(valid_map_range(range));
  std::puts("ADSB_MAP_RANGE_PASS exact protocol values; malformed/overflow input preserves prior range");
}
