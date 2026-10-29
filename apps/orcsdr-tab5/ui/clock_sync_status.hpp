#pragma once

#include <cstddef>
#include <cstdint>

// How far the UTC clock can be trusted, and when to ask the network to correct it. Pure logic (no hardware), host-tested.
//
// The hardware RTC keeps the date through power-offs but drifts about 10 ppm (roughly 0.9 s per day), and FT8/FT4 need the
// clock within a few hundred milliseconds. "The clock is set" is therefore not the same as "the clock is right": the only
// evidence of a correct clock is a recent network sync, so the status below is built from the age of the last one.
namespace orcsdr::clock_sync {

enum class Level : uint8_t {
  unsynced,   // no network sync since power-up: the RTC may be seconds off
  syncing,    // a sync is running now
  synced,     // synced recently; the estimated error is well inside what FT8 tolerates
  stale,      // the last sync is old enough that drift may matter
  failed,     // the last attempt failed and no recent sync exists
  no_wifi,    // unsynced, stale or failed, and Wi-Fi is not connected, so the person has to connect first
  signal      // no recent network sync, but the clock was corrected from the decoded stations' DT (FT8 only) and is believed right
};

constexpr uint32_t kResyncAfterSeconds = 2u * 3600u;   // the decoder asks again after this long (drift stays under about 75 ms)
constexpr uint32_t kStaleAfterSeconds = 6u * 3600u;    // beyond this the estimated drift passes 200 ms
constexpr uint32_t kRetryAfterFailureMs = 5u * 60u * 1000u;
constexpr uint32_t kRetryAfterAttemptMs = 60u * 1000u;

struct Input {
  bool clock_valid = false;        // the wall clock holds a plausible date
  bool wifi_connected = false;
  bool syncing = false;
  bool last_attempt_failed = false;
  uint32_t now_utc = 0;
  uint32_t last_sync_utc = 0;      // UTC seconds when the last successful sync finished; 0 = none since power-up
  uint32_t last_signal_sync_utc = 0;   // UTC seconds when the clock was last corrected from decoded stations; 0 = never
};

Level evaluate(const Input& in);
// Seconds since the sync the level rests on (the network sync, or the signal correction when that is the fresher evidence); 0 when
// there has been none or the clock cannot say.
uint32_t age_seconds(const Input& in);
// True while the clock is not known to be right, so the decoder should search a wider time window (it costs 3 s of delay per slot).
bool wants_wide_search(Level level);
// True when a DT-derived correction may be applied: only when no recent network sync vouches for the clock.
bool may_apply_signal_correction(Level level);

// Chip text for the dashboard. `value` is the short state word, `title` the small line above it (it carries the age and the tap hint).
void chip_text(Level level, uint32_t age_seconds, char* title, size_t title_size, char* value, size_t value_size);
// A longer sentence for the Setup row, e.g. "SYNCED 12 MIN AGO".
void long_text(Level level, uint32_t age_seconds, char* out, size_t out_size);
// The chip changes once per minute at most; this key lets the screen repaint only when the visible text can change.
uint32_t display_key(Level level, uint32_t age_seconds);

struct DueInput {
  Input status{};
  bool decoder_running = false;    // only a running decoder needs the clock; nothing is contacted otherwise
  uint32_t ms_since_last_attempt = 0xFFFFFFFFu;   // since an attempt started; max = never attempted this boot
};
// True when a sync should start now: the decoder is running, Wi-Fi is connected, none is in progress, the last sync is missing or
// older than kResyncAfterSeconds, and the retry delay since the previous attempt has passed.
bool auto_sync_due(const DueInput& in);

// A sync stamp kept across a reboot is believed only if it is plausible: not zero, not in the future (the clock was set back since) and not
// older than kRestoreMaxAgeSeconds. Anything else is dropped (0), so a restart never claims a sync that cannot be vouched for.
constexpr uint32_t kRestoreMaxAgeSeconds = 30u * 24u * 3600u;
uint32_t restore_stamp(uint32_t stored, uint32_t now_utc);

bool self_check();

}  // namespace orcsdr::clock_sync
