#include "dead_air_palette.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>

using namespace orcsdr::dead_air::viz;

int main() {
  uint16_t lut[kLutSize];
  build_lut(lut);

  // Brightness never decreases with level: colour always means "stronger".
  // (RGB565 quantisation can dip a step or two; anything larger, or any dip over a longer span, is a real defect.)
  for (int i = 1; i < kLutSize; ++i) assert(luma(lut[i]) + 3 >= luma(lut[i - 1]));
  for (int i = 8; i < kLutSize; ++i) assert(luma(lut[i]) >= luma(lut[i - 8]));
  assert(luma(lut[0]) < 12);                  // floor is near black
  assert(luma(lut[kLutSize - 1]) > 230);      // ceiling is ghost-white
  assert(luma(lut[kLutSize / 2]) > luma(lut[kLutSize / 8]));
  // Hue progression: the top end is cyan-white, the bottom is violet-black (blue/red dominate green at the floor).
  const uint16_t lo = lut[40], hi = lut[230];
  assert(((lo >> 11) & 31) * 2 >= ((lo >> 5) & 63) / 2);   // violet stage keeps red, not green
  assert(((hi >> 5) & 63) > 40 && (hi & 31) > 20);          // cyan stage is green+blue heavy

  // Fixed scale and clamping.
  assert(normalize(kFloorDbfs) == 0.0f && normalize(kCeilDbfs) == 1.0f);
  assert(normalize(-500.0f) == 0.0f && normalize(40.0f) == 1.0f);
  assert(std::fabs(normalize(-60.0f) - 0.5f) < 1e-6f);
  // Garbage never lights up the display.
  assert(normalize(std::numeric_limits<float>::quiet_NaN()) == 0.0f);
  assert(normalize(std::numeric_limits<float>::infinity()) == 0.0f);
  assert(color_at(lut, std::numeric_limits<float>::quiet_NaN()) == lut[0]);

  // Stronger bins give brighter pixels in a rendered row; interpolation stays inside the neighbouring bins.
  float bins[96];
  for (int i = 0; i < 96; ++i) bins[i] = -100.0f;
  bins[48] = -20.0f;
  uint16_t row[862];
  render_row(bins, 96, lut, row, 862);
  int brightest = 0;
  for (int x = 1; x < 862; ++x) if (luma(row[x]) > luma(row[brightest])) brightest = x;
  assert(brightest > 400 && brightest < 460);
  assert(luma(row[0]) < luma(row[brightest]) && luma(row[861]) < luma(row[brightest]));
  assert(interpolate_bins(bins, 96, 0, 862) == -100.0f && interpolate_bins(bins, 96, 861, 862) == -100.0f);
  const float mid = interpolate_bins(bins, 96, 430, 862);
  assert(mid >= -100.0f && mid <= -20.0f);

  // The retune marker is darker than any measured level above the floor and is not a flat fill.
  uint16_t gap[862];
  render_gap_row(gap, 862);
  bool varies = false;
  for (int x = 1; x < 862; ++x) {
    assert(luma(gap[x]) < luma(lut[kLutSize / 3]));
    if (gap[x] != gap[0]) varies = true;
  }
  assert(varies);

  // Peak-hold trail: rises at once, decays by a fixed amount per frame, never shows an unmeasured level.
  Trail t;
  assert(!t.valid());
  float frame[96];
  for (int i = 0; i < 96; ++i) frame[i] = -90.0f;
  frame[10] = -30.0f;
  t.update(frame, 96, 2.0f);
  assert(t.valid() && t.at(10) == -30.0f);
  frame[10] = -90.0f;
  t.update(frame, 96, 2.0f);
  assert(t.at(10) == -32.0f && t.at(11) == -90.0f);
  for (int k = 0; k < 100; ++k) t.update(frame, 96, 2.0f);
  assert(t.at(10) == -90.0f);              // fully decayed back to the measured level, never below it
  frame[3] = std::numeric_limits<float>::quiet_NaN();
  t.update(frame, 96, 2.0f);
  assert(t.at(3) == -92.0f);               // a non-finite bin counts as the floor, so the trail only decays
  t.reset();
  t.update(frame, 96, 2.0f);
  assert(t.at(3) == kFloorDbfs);
  t.reset();
  assert(!t.valid());
  std::puts("Dead Air palette tests PASS");
}
