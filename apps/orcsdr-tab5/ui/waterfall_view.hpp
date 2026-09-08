#pragma once

#include <cstddef>
#include <cstdint>

namespace orcsdr {

// A scrolling waterfall backed by a PSRAM ring buffer.
//
// M5.Display.scroll() cannot be used for this on the Tab5. The panel is a
// 720x1280 DSI framebuffer that the UI drives rotated 90 degrees, and
// Panel_FrameBufferBase::copyRect() shears the block a little on every
// rotated self-copy; after a few seconds of 10 Hz updates the waterfall has
// walked into a parallelogram that covers the sidebar and the controls row.
// Every other element on these screens renders correctly through writeImage,
// so this keeps the history in our own buffer and pushes whole rows down that
// path instead of asking the panel to blit against itself.
class WaterfallView {
 public:
  // Panel interior the history occupies, in screen pixels.
  WaterfallView(int x, int y, int w, int h);
  ~WaterfallView();

  WaterfallView(const WaterfallView&) = delete;
  WaterfallView& operator=(const WaterfallView&) = delete;

  int width() const { return w_; }

  // Drops the oldest row and returns the newest one for writing: `width()`
  // RGB565 pixels, left to right. Returns nullptr if the ring buffer could not
  // be allocated, in which case the caller should skip the waterfall entirely.
  uint16_t* next_row();

  // Blits the whole history, oldest row at the top. `visible_w` narrows the
  // blit for screens whose plot shrinks when a panel opens over it; <= 0 means
  // the full width.
  void push(int visible_w = 0);

  // Blanks the history and repaints. Call when the screen is (re)drawn.
  void clear(uint16_t color = 0);

 private:
  bool ensure();
  void push_rows(int first, int count, int screen_y, int cols);

  const int x_, y_, w_, h_;
  uint16_t* rows_ = nullptr;  // h_ rows of w_ pixels; oldest row at head_
  int head_ = 0;
  bool alloc_failed_ = false;
};

}  // namespace orcsdr
