#pragma once

#include <algorithm>
#include <cstdint>

namespace orcsdr::plot {
constexpr uint32_t kSpectrumIntervalMs = 33;

// Bounded recent-window statistics; sort only when status is requested.
// Place the instance in PSRAM on the device.
struct FrameIntervals {
  uint32_t values[128]{};
  unsigned count = 0, next = 0;
  void add(uint32_t ms) {
    values[next] = ms;
    next = (next + 1) % 128;
    if (count < 128) ++count;
  }
  uint32_t percentile(unsigned percent) const {
    if (!count || percent < 1 || percent > 100) return 0;
    uint32_t sorted[128];
    std::copy_n(values, count, sorted);
    std::sort(sorted, sorted + count);
    return sorted[(count * percent + 99) / 100 - 1];
  }
};

// Preserve the speed selector's rows per 100 ms while drawing at 33 ms.
struct WaterfallClock {
  uint32_t last_ms = 0, remainder = 0;
  void reset(uint32_t now) { last_ms = now; remainder = 0; }
  unsigned rows(uint32_t now, unsigned rows_per_100ms) {
    const uint32_t elapsed = std::min<uint32_t>(now - last_ms, 200);
    last_ms = now;
    const uint32_t scaled = elapsed * rows_per_100ms + remainder;
    remainder = scaled % 100;
    return scaled / 100;
  }
};
}  // namespace orcsdr::plot
