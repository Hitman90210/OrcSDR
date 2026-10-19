#include "../apps/orcsdr-tab5/ui/plot_cadence.hpp"
#include <cassert>
#include <cstdint>

int main() {
  using namespace orcsdr::plot;
  static_assert(kSpectrumIntervalMs == 33);
  FrameIntervals intervals;
  assert(intervals.percentile(95) == 0);
  for (uint32_t n = 1; n <= 100; ++n) intervals.add(n);
  assert(intervals.percentile(50) == 50 && intervals.percentile(95) == 95);
  for (unsigned n = 0; n < 128; ++n) intervals.add(33);
  assert(intervals.percentile(50) == 33 && intervals.percentile(95) == 33);
  for (unsigned speed : {2u, 3u, 4u}) {
    WaterfallClock clock;
    clock.reset(0);
    unsigned rows = 0;
    for (uint32_t now = 33; now <= 3300; now += 33)
      rows += clock.rows(now, speed);
    assert(rows == 33 * speed);
  }
  WaterfallClock clock;
  clock.reset(UINT32_MAX - 20);
  assert(clock.rows(79, 2) == 2);
  clock.reset(0);
  assert(clock.rows(2000, 2) == 4);
  assert(clock.rows(2001, 2) == 0);
}
