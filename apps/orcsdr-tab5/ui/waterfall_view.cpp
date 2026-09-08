#include "waterfall_view.hpp"

#include <M5Unified.h>
#include <esp_heap_caps.h>
#include <esp_log.h>

#include <algorithm>

namespace orcsdr {
namespace {
constexpr char kTag[] = "orcsdr_wf";
}  // namespace

WaterfallView::WaterfallView(int x, int y, int w, int h)
    : x_(x), y_(y), cap_w_(std::max(1, w)), h_(std::max(1, h)), w_(std::max(1, w)) {}

WaterfallView::~WaterfallView() {
  if (rows_) heap_caps_free(rows_);
}

bool WaterfallView::ensure() {
  if (rows_) return true;
  if (alloc_failed_) return false;
  // Always allocated at the widest the view can be, so set_width() never has
  // to reallocate.
  const size_t bytes = static_cast<size_t>(cap_w_) * h_ * sizeof(uint16_t);
  rows_ = static_cast<uint16_t*>(
      heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!rows_) {
    alloc_failed_ = true;
    ESP_LOGE(kTag, "waterfall %dx%d needs %u B of PSRAM, allocation failed", cap_w_,
             h_, static_cast<unsigned>(bytes));
    return false;
  }
  std::fill_n(rows_, static_cast<size_t>(cap_w_) * h_, uint16_t{0});
  head_ = 0;
  return true;
}

void WaterfallView::set_width(int w) {
  const int next = std::clamp(w, 1, cap_w_);
  if (next == w_) return;
  // Rows are packed at the active width, so the stored history is the wrong
  // shape once it changes. Discarding it keeps push() a single contiguous
  // blit per chunk instead of one call per row.
  w_ = next;
  clear();
}

uint16_t* WaterfallView::next_row() {
  if (!ensure()) return nullptr;
  // head_ is the oldest row, so it is the one the newest sample overwrites.
  uint16_t* row = rows_ + static_cast<size_t>(head_) * w_;
  head_ = (head_ + 1) % h_;
  return row;
}

void WaterfallView::push() {
  if (!rows_) return;
  // Oldest first: head_ .. h_-1, then 0 .. head_-1. Both chunks are
  // contiguous at the active stride, so each is one blit.
  const int tail = h_ - head_;
  M5.Display.pushImage(x_, y_, w_, tail,
                       reinterpret_cast<const lgfx::rgb565_t*>(
                           rows_ + static_cast<size_t>(head_) * w_));
  if (head_ > 0) {
    M5.Display.pushImage(x_, y_ + tail, w_, head_,
                         reinterpret_cast<const lgfx::rgb565_t*>(rows_));
  }
}

void WaterfallView::clear(uint16_t color) {
  if (!ensure()) {
    M5.Display.fillRect(x_, y_, w_, h_, color);
    return;
  }
  std::fill_n(rows_, static_cast<size_t>(w_) * h_, color);
  head_ = 0;
  push();
}

}  // namespace orcsdr
