#include "ntp_sync.hpp"

#include "time_service.hpp"

#include <esp_sntp.h>
#include <esp_timer.h>

#include <ctime>
#include <sys/time.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace orcsdr::ntp_sync {
namespace {

uint32_t now_ms() { return static_cast<uint32_t>(esp_timer_get_time() / 1000); }

constexpr uint32_t kTimeoutMs = 20000;
constexpr const char* kServer = "pool.ntp.org";

State g_state = State::idle;
uint32_t g_started_ms = 0;
bool g_result_pending = false;
bool g_result_ok = false;
uint32_t g_last_success_utc = 0;
bool g_attempted = false;

StepListener g_step_listener = nullptr;
timeval g_started_wall{};
int64_t g_started_mono_us = 0;

// SNTP steps the system clock by however far it was off. Tell the listener the real step (wall-clock change minus elapsed monotonic time) so
// anything keyed to the old clock (the FT8 slot schedule and its timing evidence) can restart.
void notify_step() {
  if (g_step_listener == nullptr) return;
  timeval now{};
  gettimeofday(&now, nullptr);
  const int64_t wall_ms = (static_cast<int64_t>(now.tv_sec - g_started_wall.tv_sec) * 1000000 + (now.tv_usec - g_started_wall.tv_usec)) / 1000;
  const int64_t mono_ms = (esp_timer_get_time() - g_started_mono_us) / 1000;
  const int64_t step = wall_ms - mono_ms;
  if (step > 50 || step < -50) g_step_listener(static_cast<int32_t>(step));
}

void finish(bool ok) {
  esp_sntp_stop();
  if (!ok) time_service::resync_system_from_rtc();   // SNTP already moved the system clock; put it back in step with the RTC
  if (ok) g_last_success_utc = static_cast<uint32_t>(time(nullptr));
  g_state = ok ? State::done : State::failed;
  g_result_ok = ok;
  g_result_pending = true;
}

}  // namespace

bool start(bool network_ready) {
  if (g_state == State::syncing || !network_ready) return false;
  g_result_pending = false;   // a result from an earlier attempt must not be reported for this one
  if (esp_sntp_enabled()) esp_sntp_stop();
  esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
  esp_sntp_setservername(0, kServer);
  sntp_set_sync_status(SNTP_SYNC_STATUS_RESET);
  esp_sntp_init();
  g_started_ms = now_ms();
  gettimeofday(&g_started_wall, nullptr);
  g_started_mono_us = esp_timer_get_time();
  g_attempted = true;
  g_state = State::syncing;
  return true;
}

void set_step_listener(StepListener listener) { g_step_listener = listener; }

uint32_t last_success_utc() { return g_last_success_utc; }

void restore_last_success(uint32_t utc) { g_last_success_utc = utc; }

void forget_last_success() {
  if (g_state == State::syncing) {   // an attempt in flight would finish later, rewrite the clock and mark it synced again
    esp_sntp_stop();
    time_service::resync_system_from_rtc();
    g_state = State::idle;
  }
  g_last_success_utc = 0;
  g_attempted = false;
}

uint32_t ms_since_last_attempt() { return g_attempted ? now_ms() - g_started_ms : 0xFFFFFFFFu; }

bool last_attempt_failed() { return g_attempted && g_state == State::failed; }

void poll() {
  if (g_state != State::syncing) return;
  if (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
    // SNTP has set the system clock to the millisecond, but the hardware RTC only counts whole seconds and restarts its
    // sub-second counter when it is written. Writing it mid-second would leave the RTC (and every later boot) off by the
    // fraction of a second, so wait for the next second boundary and write that second then (at most about 1 s once).
    timeval tv{};
    const bool at_boundary = time_service::wait_for_second_boundary(&tv);
    const time_t now = tv.tv_sec;
    finish(at_boundary && now > 0 && time_service::set_utc(static_cast<uint32_t>(now)));   // a missed boundary fails the sync: the RTC is not written mid-second
    notify_step();
  } else if (now_ms() - g_started_ms > kTimeoutMs) {
    finish(false);
  }
}

State state() { return g_state; }

bool take_result(bool* ok) {
  if (!g_result_pending) return false;
  g_result_pending = false;
  if (ok != nullptr) *ok = g_result_ok;
  return true;
}

}  // namespace orcsdr::ntp_sync
