#pragma once

#include <cstddef>
#include <cstdint>
#include <sys/time.h>

namespace orcsdr { class NvsStore; }

namespace orcsdr::time_service {

struct Snapshot {
  uint32_t uptime_ms = 0;
  uint32_t utc = 0;
  bool wallclock_valid = false;
};

void initialize(bool previously_established);
Snapshot now();
bool set_utc(uint32_t epoch);
// Puts the system clock back in step with the RTC (after a network sync that could not be written to it).
void resync_system_from_rtc();
// Moves the system clock by `delta_ms` (positive = later) and writes the result to the RTC. The RTC only counts whole seconds and
// restarts its sub-second counter when written, so the write waits for the next second boundary (up to about 1 s, blocking). Call from
// the UI loop, which owns the RTC. False if the clock is not valid or the RTC write did not verify; the system clock is then put back.
// `applied_ms` (optional) receives how far the wall clock really moved, on success and after a rollback alike (positive = later), so the
// caller can tell anything keyed to the old clock; it is 0 when the step was refused outright.
bool step_ms(int32_t delta_ms, int32_t* applied_ms = nullptr);
// Blocks (up to about 1 s, a little more after a late wake-up) until just after a whole-second boundary and returns that moment in `out`.
// The RTC counts whole seconds and restarts its sub-second counter when written, so it must be written right here.
// Returns false if three attempts all missed the boundary (a heavily loaded system); the caller must then not write the RTC.
bool wait_for_second_boundary(timeval* out);
bool format_utc(char* output, size_t output_size, uint32_t epoch);

// The hardware RTC always holds UTC. The zone is a UTC offset in minutes east of UTC (UTC-07:00 is -420) that
// turns it into the local time people type and read; it is applied by our own code, never through the TZ
// environment variable, and survives reboots. Daylight saving is a manual offset change.
int32_t utc_offset_minutes();
bool set_utc_offset_minutes(int32_t minutes);        // in memory only; false (and unchanged) outside UTC-12:00..UTC+14:00
// Saves, then sets, the offset: on a failed save the in-memory offset is untouched.
bool store_utc_offset_minutes(NvsStore& store, int32_t minutes);
void load_config(NvsStore& store);                   // call once after initialize()
bool save_config(NvsStore& store);
// "YYYY-MM-DD HH:MM:SS" at the configured offset (output_size >= 20).
bool format_local(char* output, size_t output_size, uint32_t epoch);
bool self_check();

}  // namespace orcsdr::time_service
