#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "clock_sync_status.hpp"

namespace {

[[noreturn]] void fail(const char* expression, int line) {
  std::fprintf(stderr, "FAIL line=%d check=%s\n", line, expression);
  std::exit(1);
}
#define CHECK(expression) do { if (!(expression)) fail(#expression, __LINE__); } while (false)

using namespace orcsdr::clock_sync;

constexpr uint32_t kNow = 1800000000u;

Input base() {
  Input in;
  in.clock_valid = true;
  in.wifi_connected = true;
  in.now_utc = kNow;
  return in;
}

void test_levels() {
  Input in = base();
  CHECK(evaluate(in) == Level::unsynced);
  in.last_attempt_failed = true;
  CHECK(evaluate(in) == Level::failed);
  in.wifi_connected = false;
  CHECK(evaluate(in) == Level::no_wifi);   // nothing to tap until Wi-Fi is up
  in.wifi_connected = true;
  in.last_attempt_failed = false;

  in.last_sync_utc = kNow - 30;
  CHECK(evaluate(in) == Level::synced && age_seconds(in) == 30);
  in.last_sync_utc = kNow - kStaleAfterSeconds;
  CHECK(evaluate(in) == Level::synced);   // exactly at the limit is still fine
  in.last_sync_utc = kNow - kStaleAfterSeconds - 1;
  CHECK(evaluate(in) == Level::stale);
  in.last_attempt_failed = true;
  CHECK(evaluate(in) == Level::stale);    // an old sync is stale even after a failed retry
  in.wifi_connected = false;
  CHECK(evaluate(in) == Level::no_wifi);

  in = base();
  in.syncing = true;
  CHECK(evaluate(in) == Level::syncing);
  in.wifi_connected = false;
  CHECK(evaluate(in) == Level::syncing);   // a running sync is shown as such

  in = base();
  in.clock_valid = false;
  in.last_sync_utc = kNow - 10;
  CHECK(evaluate(in) == Level::unsynced && age_seconds(in) == 0);   // without a valid clock the old stamp means nothing
  in = base();
  in.last_sync_utc = kNow + 100;   // clock stepped backwards
  CHECK(evaluate(in) == Level::unsynced && age_seconds(in) == 0);
}

void test_signal_correction() {
  Input in = base();
  in.wifi_connected = false;
  in.last_signal_sync_utc = kNow - 120;
  CHECK(evaluate(in) == Level::signal && age_seconds(in) == 120);   // offline but corrected from decoded stations
  in.last_signal_sync_utc = kNow - kStaleAfterSeconds - 1;
  CHECK(evaluate(in) == Level::no_wifi);                           // old evidence expires
  in.last_signal_sync_utc = kNow - 60;
  in.last_sync_utc = kNow - 30;
  CHECK(evaluate(in) == Level::synced && age_seconds(in) == 30);   // a fresh network sync outranks it
  in.last_sync_utc = kNow - kStaleAfterSeconds - 5;
  CHECK(evaluate(in) == Level::signal && age_seconds(in) == 60);   // stale network sync, fresh signal correction
  in.syncing = true;
  CHECK(evaluate(in) == Level::syncing);

  CHECK(wants_wide_search(Level::unsynced) && wants_wide_search(Level::failed) && wants_wide_search(Level::no_wifi) &&
        wants_wide_search(Level::stale));   // a sync older than 6 h may have drifted out of the normal window
  CHECK(!wants_wide_search(Level::synced) && !wants_wide_search(Level::signal) && !wants_wide_search(Level::syncing));
  CHECK(may_apply_signal_correction(Level::unsynced) && may_apply_signal_correction(Level::stale) &&
        may_apply_signal_correction(Level::signal) && may_apply_signal_correction(Level::no_wifi));
  CHECK(!may_apply_signal_correction(Level::synced) && !may_apply_signal_correction(Level::syncing));
}

void test_text() {
  char title[40];
  char value[24];
  char longer[40];
  chip_text(Level::synced, 12 * 60, title, sizeof(title), value, sizeof(value));
  CHECK(std::strcmp(value, "SYNCED 12m") == 0 && std::strcmp(title, "CLOCK - TAP") == 0);
  chip_text(Level::synced, 20, title, sizeof(title), value, sizeof(value));
  CHECK(std::strcmp(value, "SYNCED NOW") == 0);
  chip_text(Level::stale, 7 * 3600 + 100, title, sizeof(title), value, sizeof(value));
  CHECK(std::strcmp(value, "STALE 7h") == 0);
  chip_text(Level::unsynced, 0, title, sizeof(title), value, sizeof(value));
  CHECK(std::strcmp(value, "UNSYNCED") == 0);
  chip_text(Level::no_wifi, 0, title, sizeof(title), value, sizeof(value));
  CHECK(std::strcmp(value, "NO WI-FI") == 0);
  chip_text(Level::failed, 0, title, sizeof(title), value, sizeof(value));
  CHECK(std::strcmp(value, "FAILED") == 0);
  chip_text(Level::syncing, 0, title, sizeof(title), value, sizeof(value));
  CHECK(std::strcmp(value, "SYNCING") == 0 && std::strcmp(title, "CLOCK") == 0);
  // Widest cases must still fit the 190 px chip (title about 16 characters, value about 14).
  chip_text(Level::signal, 12 * 60, title, sizeof(title), value, sizeof(value));
  CHECK(std::strcmp(value, "VIA RF 12m") == 0);
  for (int l = 0; l <= 6; ++l) {
    chip_text(static_cast<Level>(l), 59 * 60, title, sizeof(title), value, sizeof(value));
    CHECK(std::strlen(title) <= 16 && std::strlen(value) <= 12);
  }

  long_text(Level::synced, 12 * 60, longer, sizeof(longer));
  CHECK(std::strcmp(longer, "SYNCED 12 MIN AGO") == 0);
  long_text(Level::stale, 7 * 3600, longer, sizeof(longer));
  CHECK(std::strcmp(longer, "STALE, SYNCED 7 H AGO") == 0);
  long_text(Level::unsynced, 0, longer, sizeof(longer));
  CHECK(std::strcmp(longer, "NOT SYNCED") == 0);

  // Tiny buffers never overflow.
  char tiny[4];
  chip_text(Level::stale, 99999, tiny, sizeof(tiny), tiny, sizeof(tiny));
  long_text(Level::stale, 99999, tiny, sizeof(tiny));
  CHECK(std::strlen(tiny) < sizeof(tiny));
}

void test_restore_stamp() {
  CHECK(restore_stamp(kNow - 600, kNow) == kNow - 600);                              // a recent sync survives a restart
  CHECK(restore_stamp(kNow - kStaleAfterSeconds - 5, kNow) == kNow - kStaleAfterSeconds - 5);   // an old one too: it then reads as stale, not synced
  CHECK(restore_stamp(0, kNow) == 0);                                                // never synced
  CHECK(restore_stamp(kNow + 1, kNow) == 0);                                         // the clock was set back since: cannot vouch
  CHECK(restore_stamp(kNow - kRestoreMaxAgeSeconds - 1, kNow) == 0);                 // too old to mean anything
  CHECK(restore_stamp(kNow - kRestoreMaxAgeSeconds, kNow) == kNow - kRestoreMaxAgeSeconds);
  CHECK(restore_stamp(kNow, 0) == 0);                                                // no valid clock yet
  // A restored stamp feeds evaluate() like a fresh one.
  Input in = base();
  in.last_sync_utc = restore_stamp(kNow - 3600, kNow);
  CHECK(evaluate(in) == Level::synced && age_seconds(in) == 3600);
}

void test_display_key() {
  CHECK(display_key(Level::synced, 100) == display_key(Level::synced, 110));       // same minute: no repaint
  CHECK(display_key(Level::synced, 100) != display_key(Level::synced, 200));       // new minute
  CHECK(display_key(Level::synced, 100) != display_key(Level::stale, 100));        // new level
  CHECK(display_key(Level::stale, 3 * 3600 + 5) == display_key(Level::stale, 3 * 3600 + 900));
  CHECK(display_key(Level::stale, 3 * 3600) != display_key(Level::stale, 4 * 3600));
}

void test_auto_sync() {
  DueInput due;
  due.status = base();
  due.decoder_running = true;
  CHECK(auto_sync_due(due));   // never synced, never tried: ask at once

  due.decoder_running = false;
  CHECK(!auto_sync_due(due));  // nothing is contacted unless the decoder needs the clock
  due.decoder_running = true;

  due.status.wifi_connected = false;
  CHECK(!auto_sync_due(due));
  due.status.wifi_connected = true;

  due.status.syncing = true;
  CHECK(!auto_sync_due(due));
  due.status.syncing = false;

  due.ms_since_last_attempt = kRetryAfterAttemptMs - 1;
  CHECK(!auto_sync_due(due));   // just tried: give it time
  due.ms_since_last_attempt = kRetryAfterAttemptMs;
  CHECK(auto_sync_due(due));

  due.status.last_attempt_failed = true;
  due.ms_since_last_attempt = kRetryAfterAttemptMs;
  CHECK(!auto_sync_due(due));   // failures back off longer
  due.ms_since_last_attempt = kRetryAfterFailureMs;
  CHECK(auto_sync_due(due));
  due.status.last_attempt_failed = false;

  due.ms_since_last_attempt = 0xFFFFFFFFu;
  due.status.last_sync_utc = kNow - (kResyncAfterSeconds - 1);
  CHECK(!auto_sync_due(due));   // recent enough
  due.status.last_sync_utc = kNow - kResyncAfterSeconds;
  CHECK(auto_sync_due(due));    // time to correct the drift

  // A clock that is not valid cannot vouch for an old sync stamp.
  due.status.clock_valid = false;
  due.status.last_sync_utc = kNow - 10;
  CHECK(auto_sync_due(due));
  // Resync interval must keep the estimated drift (10 ppm) well under the stale limit.
  CHECK(kResyncAfterSeconds < kStaleAfterSeconds);
}

}  // namespace

int main() {
  CHECK(orcsdr::clock_sync::self_check());
  test_levels();
  test_signal_correction();
  test_text();
  test_restore_stamp();
  test_display_key();
  test_auto_sync();
  std::puts("clock_sync_status_tests: all passed");
  return 0;
}
