#include "dead_air_format.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>

using namespace orcsdr::dead_air;

int main() {
  char clock[16];
  format_clock(1700000000u, 0, clock, sizeof(clock));   // 2023-11-14 22:13:20 UTC
  assert(std::string(clock) == "22:13:20");
  format_clock(0, 125000, clock, sizeof(clock));        // wall clock not verified: time since boot
  assert(std::string(clock) == "T+02:05");
  format_clock(86400u * 3u, 0, clock, sizeof(clock));   // exact midnight wraps to 00:00:00
  assert(std::string(clock) == "00:00:00" || std::string(clock) == "T+00:00");
  format_clock(1700000000u, 5, nullptr, 0);             // no crash on a null buffer

  char row[96];
  Event rf{};
  rf.sequence = 12; rf.unix_time = 1700000000u; rf.source = Source::rf; rf.kind = Kind::rf_rise;
  rf.frequency_hz = 98700000; rf.value = -2.327f; rf.reference = -35.491f;
  format_event_row(rf, row, sizeof(row));
  assert(std::string(row) == "#12 22:13:20 RF RISE -2.3 dBFS +33 dB");

  Event au = rf;
  au.sequence = 1; au.source = Source::audio; au.kind = Kind::audio_burst; au.value = -22.305f; au.reference = -47.003f;
  format_event_row(au, row, sizeof(row));
  assert(std::string(row) == "#1 22:13:20 AUDIO -22.3 dBFS +25 dB");

  Event mark{};
  mark.sequence = 3; mark.monotonic_ms = 125000; mark.source = Source::manual; mark.kind = Kind::manual_marker; mark.frequency_hz = 97289000;
  format_event_row(mark, row, sizeof(row));
  assert(std::string(row) == "#3 T+02:05 MARK 97.289 MHz");   // a mark carries no measured value, and none is shown

  Event nomark = mark;
  nomark.frequency_hz = 0;
  format_event_row(nomark, row, sizeof(row));
  assert(std::string(row) == "#3 T+02:05 MARK no receiver");   // never "0.000 MHz"

  Event sw{};
  sw.sequence = 4; sw.unix_time = 1700000000u; sw.source = Source::sweep; sw.kind = Kind::sweep_start; sw.frequency_hz = 88100000;
  format_event_row(sw, row, sizeof(row));
  assert(std::string(row) == "#4 22:13:20 SWEEP START 88.100 MHz");
  sw.kind = Kind::sweep_stop; sw.sweep_hops = 17;
  format_event_row(sw, row, sizeof(row));
  assert(std::string(row) == "#4 22:13:20 SWEEP STOP 17 hops");

  // A short buffer is truncated, never overrun.
  char tiny[8];
  format_event_row(rf, tiny, sizeof(tiny));
  assert(std::strlen(tiny) < sizeof(tiny));
  std::puts("Dead Air format tests PASS");
}
