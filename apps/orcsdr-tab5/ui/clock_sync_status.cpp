#include "clock_sync_status.hpp"

#include <cstdio>
#include <cstring>

namespace orcsdr::clock_sync {
namespace {

void format_age_long(char* out, size_t size, uint32_t seconds) {
  if (seconds < 90u) std::snprintf(out, size, "JUST NOW");
  else if (seconds < 3600u) std::snprintf(out, size, "%u MIN AGO", static_cast<unsigned>((seconds + 30u) / 60u));
  else std::snprintf(out, size, "%u H AGO", static_cast<unsigned>(seconds / 3600u));
}

void format_age_short(char* out, size_t size, uint32_t seconds) {
  if (seconds < 90u) std::snprintf(out, size, "NOW");
  else if (seconds < 3600u) std::snprintf(out, size, "%um", static_cast<unsigned>((seconds + 30u) / 60u));
  else std::snprintf(out, size, "%uh", static_cast<unsigned>(seconds / 3600u));
}

}  // namespace

namespace {

bool have_stamp(const Input& in, uint32_t stamp) { return stamp != 0 && in.clock_valid && in.now_utc >= stamp; }

}  // namespace

uint32_t age_seconds(const Input& in) {
  const bool network = have_stamp(in, in.last_sync_utc);
  const bool signal = have_stamp(in, in.last_signal_sync_utc);
  const uint32_t network_age = network ? in.now_utc - in.last_sync_utc : 0;
  const uint32_t signal_age = signal ? in.now_utc - in.last_signal_sync_utc : 0;
  if (network && network_age <= kStaleAfterSeconds) return network_age;
  if (signal && signal_age <= kStaleAfterSeconds) return signal_age;
  return network ? network_age : 0;
}

bool wants_wide_search(Level level) {
  // A sync older than the stale limit may have drifted out of the normal window (about 0.9 s a day), so it searches wide too.
  return level == Level::unsynced || level == Level::failed || level == Level::no_wifi || level == Level::stale;
}

bool may_apply_signal_correction(Level level) {
  return level != Level::synced && level != Level::syncing;
}

Level evaluate(const Input& in) {
  if (in.syncing) return Level::syncing;
  const bool have_sync = have_stamp(in, in.last_sync_utc);
  if (have_sync && in.now_utc - in.last_sync_utc <= kStaleAfterSeconds) return Level::synced;
  if (have_stamp(in, in.last_signal_sync_utc) && in.now_utc - in.last_signal_sync_utc <= kStaleAfterSeconds) return Level::signal;
  if (have_sync) return in.wifi_connected ? Level::stale : Level::no_wifi;
  if (!in.wifi_connected) return Level::no_wifi;
  return in.last_attempt_failed ? Level::failed : Level::unsynced;
}

void chip_text(Level level, uint32_t age, char* title, size_t title_size, char* value, size_t value_size) {
  // The chip is 190 px wide: the title (font 0) holds about 16 characters and the value (size 2) about 14.
  char age_text[16]{};
  format_age_short(age_text, sizeof(age_text), age);
  std::snprintf(title, title_size, level == Level::syncing ? "CLOCK" : "CLOCK - TAP");
  switch (level) {
    case Level::synced: std::snprintf(value, value_size, "SYNCED %s", age_text); break;
    case Level::syncing: std::snprintf(value, value_size, "SYNCING"); break;
    case Level::stale: std::snprintf(value, value_size, "STALE %s", age_text); break;
    case Level::failed: std::snprintf(value, value_size, "FAILED"); break;
    case Level::no_wifi: std::snprintf(value, value_size, "NO WI-FI"); break;
    case Level::signal: std::snprintf(value, value_size, "VIA RF %s", age_text); break;
    case Level::unsynced:
    default: std::snprintf(value, value_size, "UNSYNCED"); break;
  }
}

void long_text(Level level, uint32_t age, char* out, size_t size) {
  char age_text[24]{};
  format_age_long(age_text, sizeof(age_text), age);
  switch (level) {
    case Level::synced: std::snprintf(out, size, "SYNCED %s", age_text); break;
    case Level::syncing: std::snprintf(out, size, "SYNCING..."); break;
    case Level::stale: std::snprintf(out, size, "STALE, SYNCED %s", age_text); break;
    case Level::failed: std::snprintf(out, size, "SYNC FAILED"); break;
    case Level::no_wifi: std::snprintf(out, size, "NOT SYNCED, NO WI-FI"); break;
    case Level::signal: std::snprintf(out, size, "FROM RF SIGNALS %s", age_text); break;
    case Level::unsynced:
    default: std::snprintf(out, size, "NOT SYNCED"); break;
  }
}

uint32_t display_key(Level level, uint32_t age) {
  // The text changes when the level changes or the age bucket changes (minutes under an hour, then hours).
  const uint32_t bucket = age < 3600u ? (age + 30u) / 60u : 100u + age / 3600u;
  return (static_cast<uint32_t>(level) << 24) | (bucket & 0xFFFFFFu);
}

bool auto_sync_due(const DueInput& in) {
  if (!in.decoder_running || !in.status.wifi_connected || in.status.syncing) return false;
  const bool have_sync = in.status.last_sync_utc != 0 && in.status.clock_valid && in.status.now_utc >= in.status.last_sync_utc;
  if (have_sync && in.status.now_utc - in.status.last_sync_utc < kResyncAfterSeconds) return false;
  const uint32_t wait = in.status.last_attempt_failed ? kRetryAfterFailureMs : kRetryAfterAttemptMs;
  return in.ms_since_last_attempt >= wait;
}

uint32_t restore_stamp(uint32_t stored, uint32_t now_utc) {
  if (stored == 0 || now_utc == 0 || stored > now_utc || now_utc - stored > kRestoreMaxAgeSeconds) return 0;
  return stored;
}

bool self_check() {
  Input in{};
  in.clock_valid = true;
  in.wifi_connected = true;
  in.now_utc = 1800000000u;
  if (evaluate(in) != Level::unsynced) return false;
  in.last_attempt_failed = true;
  if (evaluate(in) != Level::failed) return false;
  in.last_attempt_failed = false;
  in.last_sync_utc = in.now_utc - 600u;
  if (evaluate(in) != Level::synced || age_seconds(in) != 600u) return false;
  in.last_sync_utc = in.now_utc - kStaleAfterSeconds - 1u;
  if (evaluate(in) != Level::stale) return false;
  in.wifi_connected = false;
  if (evaluate(in) != Level::no_wifi) return false;
  in.syncing = true;
  return evaluate(in) == Level::syncing;
}

}  // namespace orcsdr::clock_sync
