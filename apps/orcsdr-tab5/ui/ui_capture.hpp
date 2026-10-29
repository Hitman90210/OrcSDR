#pragma once

#include "orcsdr_storage.hpp"
#include <M5GFX.h>

#include <cstddef>
#include <cstdint>

namespace orcsdr::ui_capture {

struct Result {
  bool ok = false;
  size_t bytes = 0;
  uint16_t width = 0;
  uint16_t height = 0;
  uint8_t sha256[32]{};
  const char* error = nullptr;
};

bool valid_slug(const char* slug);
Result save_bmp(M5GFX& display, orcsdr::storage::FileSystem& filesystem, const char* slug);

// Receives the text the capture produces (the application passes a function that writes it to the serial port, blocking until sent).
// It returns false when the data could not be sent (the host stopped reading): the capture then stops at once instead of waiting out every chunk.
using SnapSink = bool (*)(const char* data, size_t size, void* context);

// Sends the screen as a PNG through `sink` (the serial port) without the SD card and without stopping the radio or any other task:
//   ORC_SNAP_BEGIN width=1280 height=720 bytes=<n> sha256=<hex>
//   ORC_SNAP_DATA <offset> <base64>          (up to kSnapChunkBytes of PNG per line, in order)
//   ORC_SNAP_END
// or one ORC_SNAP_ERROR <reason> line. The PNG comes from M5GFX's own encoder (readRectRGB one row at a time into PSRAM); the caller is
// busy for the encode (a second or so on a dashboard) and for the transfer, nothing else is paused.
constexpr size_t kSnapChunkBytes = 768;
bool stream_png(M5GFX& display, SnapSink sink, void* context);

}  // namespace orcsdr::ui_capture
