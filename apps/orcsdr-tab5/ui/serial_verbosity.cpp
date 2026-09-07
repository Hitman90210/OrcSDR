#include "serial_verbosity.hpp"

#include <atomic>
#include <strings.h>

#include <esp_log.h>

namespace {
std::atomic<uint8_t> g_serial_verbosity{static_cast<uint8_t>(SerialVerbosity::normal)};
}

bool serial_verbosity_at(SerialVerbosity level) {
  return g_serial_verbosity.load(std::memory_order_relaxed) >= static_cast<uint8_t>(level);
}

SerialVerbosity current_serial_verbosity() {
  return static_cast<SerialVerbosity>(g_serial_verbosity.load(std::memory_order_relaxed));
}

const char* serial_verbosity_name(SerialVerbosity level) {
  switch (level) {
    case SerialVerbosity::quiet: return "QUIET";
    case SerialVerbosity::normal: return "NORMAL";
    case SerialVerbosity::debug: return "DEBUG";
    case SerialVerbosity::trace: return "TRACE";
  }
  return "NORMAL";
}

bool parse_serial_verbosity(const char* text, SerialVerbosity* out) {
  if (text == nullptr || out == nullptr) return false;
  if (strcasecmp(text, "QUIET") == 0) *out = SerialVerbosity::quiet;
  else if (strcasecmp(text, "NORMAL") == 0) *out = SerialVerbosity::normal;
  else if (strcasecmp(text, "DEBUG") == 0) *out = SerialVerbosity::debug;
  else if (strcasecmp(text, "TRACE") == 0) *out = SerialVerbosity::trace;
  else return false;
  return true;
}

void apply_serial_verbosity(SerialVerbosity level) {
  g_serial_verbosity.store(static_cast<uint8_t>(level), std::memory_order_relaxed);
  const esp_log_level_t esp_level =
      level == SerialVerbosity::quiet ? ESP_LOG_ERROR
      : level == SerialVerbosity::normal ? ESP_LOG_INFO
      : level == SerialVerbosity::debug ? ESP_LOG_DEBUG : ESP_LOG_VERBOSE;
  esp_log_level_set("*", esp_level);
}
