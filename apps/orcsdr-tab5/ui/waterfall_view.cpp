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
    : x_(x), y_(y), w_(std::max(1, w)), h_(std::max(1, h)) {}

WaterfallView::~WaterfallView() {
  if (rows_) heap_caps_free(rows_);
}

bool WaterfallView::ensure() {
  if (rows_) return true;
  if (alloc_failed_) return false;
  const size_t bytes = static_cast<size_t>(w_) * h_ * sizeof(uint16_t);
  rows_ = static_cast<uint16_t*>(
      heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!rows_) {
    alloc_failed_ = true;
    ESP_LOGE(kTag, "waterfall %dx%d needs %u B of PSRAM, allocation failed", w_,
             h_, static_cast<unsigned>(bytes));
    return false;
  }
  std::fill_n(rows_, static_cast<size_t>(w_) * h_, uint16_t{0});
  head_ = 0;
  return true;
}

uint16_t* WaterfallView::next_row() {
  if (!ensure()) return nullptr;
  // head_ is the oldest row, so it is the one the newest sample overwrites.
  uint16_t* row = rows_ + static_cast<size_t>(head_) * w_;
  head_ = (head_ + 1) % h_;
  return row;
}

void WaterfallView::push(int visible_w) {
  if (!rows_) return;
  const int cols = (visible_w > 0 && visible_w < w_) ? visible_w : w_;
  // Oldest first: head_ .. h_-1, then 0 .. head_-1. Each chunk is contiguous,
  // so a narrowed blit only needs pushImageRect to skip the unused tail of
  // every stored row.
  const int tail = h_ - head_;
  push_rows(head_, tail, y_, cols);
  if (head_ > 0) push_rows(0, head_, y_ + tail, cols);
}

void WaterfallView::push_rows(int first, int count, int screen_y, int cols) {
  const uint16_t* src = rows_ + static_cast<size_t>(first) * w_;
  if (cols == w_) {
    M5.Display.pushImage(x_, screen_y, w_, count,
                         reinterpret_cast<const lgfx::rgb565_t*>(src));
    return;
  }
  for (int i = 0; i < count; ++i) {
    M5.Display.pushImage(
        x_, screen_y + i, cols, 1,
        reinterpret_cast<const lgfx::rgb565_t*>(src + static_cast<size_t>(i) * w_));
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
