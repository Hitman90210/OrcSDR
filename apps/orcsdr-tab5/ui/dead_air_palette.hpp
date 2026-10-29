#pragma once
// Dead Air "ectoplasm" look for the spectrum and waterfall: black-violet through violet and spectral cyan to
// ghost-white. The style is only a colour map over real data. Brightness rises monotonically with level, the scale is
// fixed (so frames are comparable) and shown in a legend, and nothing is drawn where there is no measured spectrum.
// Pure functions, no display access, host-tested.
#include <cstddef>
#include <cstdint>

namespace orcsdr::dead_air::viz {

constexpr float kFloorDbfs = -110.0f;  // bottom of the fixed colour scale and of the trace plot
constexpr float kCeilDbfs = -10.0f;    // top of the scale
constexpr int kLutSize = 256;

uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b);
uint8_t luma(uint16_t rgb565_color);  // approximate perceived brightness 0..255, for tests and the glow rule

// 0..1 position of a level on the fixed scale. Non-finite input maps to 0 (the dark end), never to a bright colour.
float normalize(float dbfs);

// Fills `lut[kLutSize]` with the palette.
void build_lut(uint16_t* lut);
uint16_t color_at(const uint16_t* lut, float dbfs);

// Linear interpolation across `count` bins for pixel column x of `width`.
float interpolate_bins(const float* bins, size_t count, int x, int width);

// One waterfall row (`width` pixels) from a real spectrum frame, and the marker row written at a retune.
void render_row(const float* bins, size_t count, const uint16_t* lut, uint16_t* row, int width);
void render_gap_row(uint16_t* row, int width);

// Peak-hold afterimage: follows rises immediately and falls `decay_db` per frame. It is derived from the same bins, so
// it never shows a level that was not measured.
class Trail {
 public:
  static constexpr size_t kMax = 256;
  void reset() { have_ = false; }
  void update(const float* bins, size_t count, float decay_db);
  bool valid() const { return have_; }
  float at(size_t i) const { return i < count_ ? value_[i] : kFloor; }
  size_t count() const { return count_; }
 private:
  static constexpr float kFloor = -200.0f;
  float value_[kMax]{};
  size_t count_ = 0;
  bool have_ = false;
};

}  // namespace orcsdr::dead_air::viz
