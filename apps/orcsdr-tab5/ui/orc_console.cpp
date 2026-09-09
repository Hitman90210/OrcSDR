#include "orc_console.hpp"

#include <driver/usb_serial_jtag.h>
#include <freertos/FreeRTOS.h>

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace orcsdr {

void OrcConsole::begin(uint32_t) {
  if (usb_serial_jtag_is_driver_installed()) return;
  usb_serial_jtag_driver_config_t config = {
      .tx_buffer_size = 4096,
      .rx_buffer_size = 1024,
  };
  usb_serial_jtag_driver_install(&config);
}

int OrcConsole::available() {
  if (has_pending_) return 1;
  // A zero-tick poll can miss a packet handed off just after this iteration.
  // One RTOS tick keeps the CLI responsive without blocking radio/DSP work.
  has_pending_ = usb_serial_jtag_read_bytes(&pending_, 1, pdMS_TO_TICKS(1)) == 1;
  return has_pending_ ? 1 : 0;
}

int OrcConsole::read() {
  if (!has_pending_) return -1;
  has_pending_ = false;
  return pending_;
}

size_t OrcConsole::readBytes(uint8_t* output, size_t size, uint32_t timeout_ms) {
  if (output == nullptr || size == 0) return 0;
  size_t received = 0;
  if (has_pending_) {
    output[received++] = pending_;
    has_pending_ = false;
  }
  while (received < size) {
    const int count = usb_serial_jtag_read_bytes(
        output + received, size - received, pdMS_TO_TICKS(timeout_ms));
    if (count <= 0) break;
    received += static_cast<size_t>(count);
  }
  return received;
}

void OrcConsole::print(char value) { write(&value, 1); }

void OrcConsole::print(const char* value) {
  if (value != nullptr) write(value, std::strlen(value));
}

void OrcConsole::println() { print('\n'); }

void OrcConsole::println(const char* value) {
  print(value);
  println();
}

void OrcConsole::printf(const char* format, ...) {
  char output[2048];
  va_list args;
  va_start(args, format);
  const int length = std::vsnprintf(output, sizeof(output), format, args);
  va_end(args);
  if (length > 0) {
    write(output, std::min(static_cast<size_t>(length), sizeof(output) - 1));
  }
}

size_t OrcConsole::writeBytes(const uint8_t* data, size_t size) {
  if (data == nullptr) return 0;
  size_t written = 0;
  while (written < size) {
    const int count = usb_serial_jtag_write_bytes(
        data + written, size - written, pdMS_TO_TICKS(3000));
    if (count <= 0) break;
    written += static_cast<size_t>(count);
  }
  return written;
}

void OrcConsole::write(const void* data, size_t size) {
  if (data == nullptr || size == 0) return;
  usb_serial_jtag_write_bytes(data, size, pdMS_TO_TICKS(100));
}

}  // namespace orcsdr
