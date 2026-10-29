#pragma once
// Plain-text rendering of evidence-log rows for the Dead Air screens. Pure and host-tested. Nothing here invents data:
// a row shows only what the event record holds, and an unverified wall clock is shown as time since boot.
#include <cstddef>
#include <cstdint>

#include "dead_air_model.hpp"

namespace orcsdr::dead_air {

// "22:13:20" (UTC) when the wall clock was verified (unix_time != 0), otherwise "T+02:05" (time since boot).
void format_clock(uint32_t unix_time, uint32_t monotonic_ms, char* out, size_t capacity);

// One event-log row, for example "#12 22:13:20 RF RISE -2.3 dBFS +33 dB" or "#3 T+02:05 MARK 97.289 MHz".
void format_event_row(const Event& event, char* out, size_t capacity);

}  // namespace orcsdr::dead_air
