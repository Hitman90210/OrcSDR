#pragma once

#include <cstdint>

// Optional network time. It runs when the person asks (Settings > System > SET CLOCK > SYNC NTP, or the CLOCK chip on the
// FT8 screen) and, while the FT8/FT4 decoder is running, when clock_sync::auto_sync_due() says the last sync is old. In both
// cases Wi-Fi must already be connected; the clock never depends on Internet access. A successful sync writes UTC to the
// hardware RTC through time_service::set_utc().
namespace orcsdr::ntp_sync {

enum class State : uint8_t { idle, syncing, done, failed };

// Starts one sync attempt. False if one is already running or the network is not up (`network_ready`, the caller's
// Wi-Fi state): nothing is contacted unless Wi-Fi is already connected.
bool start(bool network_ready);
// Call from the UI loop; finishes the attempt (or times it out after 20 s).
void poll();
State state();
// True once per finished attempt; `ok` says whether the RTC was set.
bool take_result(bool* ok);
// Called after a sync with how far the wall clock actually moved (ms, positive = later) when that was more than 50 ms.
using StepListener = void (*)(int32_t delta_ms);
void set_step_listener(StepListener listener);
// UTC seconds when the last sync succeeded (0 = none since power-up).
uint32_t last_success_utc();
// Milliseconds since the last attempt started (0xFFFFFFFF = none since power-up) and whether that attempt failed.
uint32_t ms_since_last_attempt();
bool last_attempt_failed();
// Boot: put back the stamp of the last successful sync that was kept in flash (already checked by clock_sync::restore_stamp).
void restore_last_success(uint32_t utc);
// Bench use (FT8 CLOCKTEST): forget that a sync ever succeeded, so the clock reads as unsynced.
void forget_last_success();

}  // namespace orcsdr::ntp_sync
