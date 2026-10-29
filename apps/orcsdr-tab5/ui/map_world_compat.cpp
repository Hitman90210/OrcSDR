#include "map_world_compat.hpp"
#include "ft8_world_data.hpp"
#include <M5GFX.h>
#include <esp_heap_caps.h>
#include <algorithm>
#include <cmath>
namespace orcsdr::maps {
void draw_world_compat(lgfx::LovyanGFX& d, int sub_x, int sub_y, int left, int top, int width, int height) {
  constexpr uint16_t kWater = 0x1105, kLand = 0x1082, kCoast = 0x2A2A, kBorder = 0x2124;
  d.fillRect(left-sub_x,top-sub_y,width,height,kWater);
  // Projected points for this frame, 16 KB: kept in PSRAM (allocated once) because internal RAM is scarce.
  static int16_t* projected = static_cast<int16_t*>(heap_caps_malloc(2u * 4096u * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (projected == nullptr) {
    d.setTextDatum(top_left);d.setTextSize(1);d.setTextColor(kCoast);
    d.drawString("No world outline: low memory. Grid positions remain available.",left-sub_x+4,top-sub_y+4);
    return;
  }
  int16_t* const sx = projected;
  int16_t* const sy = projected + 4096;
  const size_t points = std::min<size_t>(ft8::kWorldPointCount, 4096);
  for (size_t i = 0; i < points; ++i) {
    int x = 0, y = 0;
    world_compat_project(ft8::kWorldPoints[2 * i + 1] * 0.01f, ft8::kWorldPoints[2 * i] * 0.01f, left, top, width, height, &x, &y);
    sx[i] = static_cast<int16_t>(std::max(-30000, std::min(30000, x)));
    sy[i] = static_cast<int16_t>(std::max(-30000, std::min(30000, y)));
  }
  d.fillRect(left - sub_x, top - sub_y, width, height, kWater);
  d.setClipRect(left - sub_x, top - sub_y, width, height);
  for (int y = top; y < top + height; ++y) {
    int xs[96];
    int count = 0;
    const int yc = y * 2 + 1;   // sample at the pixel centre in half-pixel units
    for (size_t s = 0; s < ft8::kWorldSegmentCount && count < 94; ++s) {
      const ft8::WorldSegment& seg = ft8::kWorldSegments[s];
      if (seg.kind != 0) continue;
      for (uint16_t i = 0; i + 1 < seg.count && count < 94; ++i) {
        const size_t a = seg.start + i, b = a + 1;
        const int y0 = sy[a] * 2, y1 = sy[b] * 2;
        if ((y0 <= yc) == (y1 <= yc)) continue;
        xs[count++] = sx[a] + static_cast<int>(static_cast<int32_t>(sx[b] - sx[a]) * (yc - y0) / (y1 - y0));
      }
    }
    for (int i = 1; i < count; ++i) {   // insertion sort: few crossings per row
      const int v = xs[i];
      int j = i - 1;
      while (j >= 0 && xs[j] > v) { xs[j + 1] = xs[j]; --j; }
      xs[j + 1] = v;
    }
    for (int i = 0; i + 1 < count; i += 2) {
      const int x0 = std::max(left, xs[i]), x1 = std::min(left + width - 1, xs[i + 1]);
      if (x1 >= x0) d.drawFastHLine(x0 - sub_x, y - sub_y, x1 - x0 + 1, kLand);
    }
  }
  for (size_t s = 0; s < ft8::kWorldSegmentCount; ++s) {
    const ft8::WorldSegment& seg = ft8::kWorldSegments[s];
    for (uint16_t i = 0; i + 1 < seg.count; ++i) {
      const size_t a = seg.start + i, b = a + 1;
      if (a >= points || b >= points) break;
      d.drawLine(sx[a] - sub_x, sy[a] - sub_y, sx[b] - sub_x, sy[b] - sub_y, seg.kind == 0 ? kCoast : kBorder);
    }
  }
  d.clearClipRect();
  d.setFont(nullptr);d.setTextDatum(bottom_left);d.setTextSize(1);d.setTextColor(kCoast,kWater);
  d.drawString("Natural Earth (public domain); legacy full-world projection",left-sub_x+3,top-sub_y+height-2);
}

} // namespace orcsdr::maps
