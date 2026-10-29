#include "dead_air_palette.hpp"

#include <algorithm>
#include <cmath>

namespace orcsdr::dead_air::viz {
namespace {
struct Stop { float t; uint8_t r, g, b; };
// Luminance rises at every stop: black-violet, deep violet, violet, spectral teal, cyan, ghost-white.
constexpr Stop kStops[] = {
    {0.00f, 4, 2, 14},    {0.18f, 28, 8, 70},   {0.38f, 74, 30, 150},
    {0.58f, 22, 138, 190}, {0.80f, 112, 240, 236}, {1.00f, 244, 255, 255},
};
constexpr int kStopCount = sizeof(kStops) / sizeof(kStops[0]);
}  // namespace

uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return static_cast<uint16_t>(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

uint8_t luma(uint16_t c) {
  const int r = ((c >> 11) & 31) * 255 / 31, g = ((c >> 5) & 63) * 255 / 63, b = (c & 31) * 255 / 31;
  return static_cast<uint8_t>((r * 54 + g * 183 + b * 18) >> 8);
}

float normalize(float dbfs) {
  if (!std::isfinite(dbfs)) return 0.0f;
  return std::clamp((dbfs - kFloorDbfs) / (kCeilDbfs - kFloorDbfs), 0.0f, 1.0f);
}

void build_lut(uint16_t* lut) {
  if (!lut) return;
  for (int i = 0; i < kLutSize; ++i) {
    const float t = static_cast<float>(i) / static_cast<float>(kLutSize - 1);
    int k = 0;
    while (k + 2 < kStopCount && t > kStops[k + 1].t) ++k;
    const Stop& a = kStops[k];
    const Stop& b = kStops[k + 1];
    const float f = std::clamp((t - a.t) / (b.t - a.t), 0.0f, 1.0f);
    const auto mix = [f](uint8_t x, uint8_t y) { return static_cast<uint8_t>(x + (y - x) * f + 0.5f); };
    lut[i] = rgb565(mix(a.r, b.r), mix(a.g, b.g), mix(a.b, b.b));
  }
}

uint16_t color_at(const uint16_t* lut, float dbfs) {
  return lut[static_cast<int>(normalize(dbfs) * (kLutSize - 1) + 0.5f)];
}

float interpolate_bins(const float* bins, size_t count, int x, int width) {
  if (!bins || count == 0 || width <= 0) return kFloorDbfs;
  if (count == 1 || width == 1) return bins[0];
  const float u = static_cast<float>(x) * static_cast<float>(count - 1) / static_cast<float>(width - 1);
  const size_t i = std::min(static_cast<size_t>(u), count - 2);
  const float f = u - static_cast<float>(i);
  return bins[i] + (bins[i + 1] - bins[i]) * f;
}

void render_row(const float* bins, size_t count, const uint16_t* lut, uint16_t* row, int width) {
  if (!row || !lut) return;
  for (int x = 0; x < width; ++x) row[x] = color_at(lut, interpolate_bins(bins, count, x, width));
}

void render_gap_row(uint16_t* row, int width) {
  if (!row) return;
  // Dotted dim violet: visibly "not data", and darker than any measured level above the floor.
  const uint16_t dim = rgb565(40, 24, 72), dark = rgb565(10, 6, 24);
  for (int x = 0; x < width; ++x) row[x] = (x / 3) % 2 ? dim : dark;
}

void Trail::update(const float* bins, size_t count, float decay_db) {
  count_ = std::min(count, kMax);
  for (size_t i = 0; i < count_; ++i) {
    const float v = std::isfinite(bins[i]) ? bins[i] : kFloorDbfs;
    value_[i] = have_ ? std::max(v, value_[i] - decay_db) : v;
  }
  have_ = true;
}

}  // namespace orcsdr::dead_air::viz
