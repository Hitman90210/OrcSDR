#include "dead_air_format.hpp"

#include <cstdio>

namespace orcsdr::dead_air {

void format_clock(uint32_t unix_time, uint32_t monotonic_ms, char* out, size_t capacity) {
  if (!out || capacity == 0) return;
  if (unix_time != 0) {
    const uint32_t day = unix_time % 86400u;
    std::snprintf(out, capacity, "%02lu:%02lu:%02lu", static_cast<unsigned long>(day / 3600u),
                  static_cast<unsigned long>(day / 60u % 60u), static_cast<unsigned long>(day % 60u));
  } else {
    const uint32_t s = monotonic_ms / 1000u;
    std::snprintf(out, capacity, "T+%02lu:%02lu", static_cast<unsigned long>(s / 60u % 100u), static_cast<unsigned long>(s % 60u));
  }
}

void format_event_row(const Event& e, char* out, size_t capacity) {
  if (!out || capacity == 0) return;
  char clock[16];
  format_clock(e.unix_time, e.monotonic_ms, clock, sizeof(clock));
  const unsigned long seq = static_cast<unsigned long>(e.sequence);
  // A mark or sweep record made with no receiver tuned says so, instead of showing a frequency that never existed.
  char where[24];
  if (e.frequency_hz) std::snprintf(where, sizeof(where), "%.3f MHz", static_cast<double>(e.frequency_hz) / 1e6);
  else std::snprintf(where, sizeof(where), "no receiver");
  switch (e.kind) {
    case Kind::rf_rise:
    case Kind::audio_burst:
      std::snprintf(out, capacity, "#%lu %s %s %.1f dBFS +%.0f dB", seq, clock, e.kind == Kind::rf_rise ? "RF RISE" : "AUDIO",
                    static_cast<double>(e.value), static_cast<double>(e.value - e.reference));
      break;
    case Kind::manual_marker:
      std::snprintf(out, capacity, "#%lu %s MARK %s", seq, clock, where);
      break;
    case Kind::sweep_start:
      std::snprintf(out, capacity, "#%lu %s SWEEP START %s", seq, clock, where);
      break;
    case Kind::sweep_stop:
      std::snprintf(out, capacity, "#%lu %s SWEEP STOP %lu hops", seq, clock, static_cast<unsigned long>(e.sweep_hops));
      break;
    default:
      std::snprintf(out, capacity, "#%lu %s %s", seq, clock, kind_name(e.kind));
      break;
  }
}

}  // namespace orcsdr::dead_air
