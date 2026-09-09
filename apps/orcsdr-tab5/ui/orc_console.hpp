#pragma once

#include <cstddef>
#include <cstdint>

namespace orcsdr {

// Small buffered adapter around the ESP32-P4 USB Serial/JTAG driver. Keeping
// transport mechanics out of main.cpp makes the CLI independently testable and
// prevents radio/UI code from depending directly on the IDF driver API.
class OrcConsole {
 public:
  void begin(uint32_t baud_rate);
  int available();
  int read();
  size_t readBytes(uint8_t* output, size_t size, uint32_t timeout_ms);
  void print(char value);
  void print(const char* value);
  void println();
  void println(const char* value);
  void printf(const char* format, ...);
  size_t writeBytes(const uint8_t* data, size_t size);

 private:
  void write(const void* data, size_t size);

  uint8_t pending_ = 0;
  bool has_pending_ = false;
};

}  // namespace orcsdr
