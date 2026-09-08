#pragma once

#include <cstddef>
#include <cstdint>

namespace orcsdr {

// A scrolling waterfall backed by a PSRAM ring buffer.
//
// M5.Display.scroll() cannot be used for this. The scroll rect is *global*
// display state, and the screens that scroll a waterfall each set it somewhere
// else -- the radio screen in main.cpp only set it when the nav panel opened,
// so it spent the rest of the time scrolling against whichever dashboard had
// touched the rect last. Keeping the history here and pushing whole rows
// through writeImage (the path every other element on these screens already
// renders through) removes the shared state entirely.
class WaterfallView {
 public:
  // Panel interior the history occupies, in screen pixels. `w` is also the
  // widest the view can become; set_width() may narrow it later.
  WaterfallView(int x, int y, int w, int h);
  ~WaterfallView();

  WaterfallView(const WaterfallView&) = delete;
  WaterfallView& operator=(const WaterfallView&) = delete;

  int width() const { return w_; }

  // Narrows (or restores) the active width, for screens whose plot shrinks
  // when a panel opens over it. Rows are stored at the active width, so a
  // change discards the history rather than rescaling it. No-op if unchanged.
  void set_width(int w);

  // Drops the oldest row and returns the newest one for writing: `width()`
  // RGB565 pixels, left to right. Returns nullptr if the ring buffer could not
  // be allocated, in which case the caller should skip the waterfall entirely.
  uint16_t* next_row();

  // Blits the whole history, oldest row at the top.
  void push();

  // Blanks the history and repaints. Call when the screen is (re)drawn.
  void clear(uint16_t color = 0);

 private:
  bool ensure();

  const int x_, y_, cap_w_, h_;
  int w_;                     // active width, <= cap_w_
  uint16_t* rows_ = nullptr;  // h_ rows of w_ pixels; oldest row at head_
  int head_ = 0;
  bool alloc_failed_ = false;
};

}  // namespace orcsdr
