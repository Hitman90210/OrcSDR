#pragma once

#include <cstddef>
#include <cstdint>

#include "pocsag_decoder_core.hpp"

// M5GFX-only POCSAG pager dashboard: consumes a bounded Snapshot, emits
// Action values, and never touches esp_rtl_sdr, SD, or decoder internals
// directly -- mirrors adsb_dashboard.hpp's enter/leave/draw/update/
// set_live_snapshot/handle_touch shape.
namespace orcsdr::pocsag {

struct Settings {
  uint32_t frequency_hz = 0;
  // 0 = AUTO; otherwise a manual override matching Baud's b512/b1200/b2400.
  uint16_t baud_bps = 0;
  // 0 = AUTO, 1 = NORMAL, 2 = INVERTED.
  uint8_t polarity_mode = 0;
};

// Matches the LIVE view's visible row count (draw_live() shows at most 12);
// deeper history belongs in the SD-backed archive (Phase 10.3), not a larger
// RAM-resident snapshot.
constexpr size_t kRecentMessageCapacity = 12;

struct DisplayMessage {
  uint64_t timestamp_ms = 0;
  uint32_t capcode = 0;
  uint8_t function = 0;
  MessageType type = MessageType::unknown;
  uint16_t baud = 0;
  bool inverted = false;
  bool truncated = false;
  uint16_t corrected_bits = 0;
  uint16_t uncorrectable_words = 0;
  char text[kMaxMessageChars] = {};
  uint16_t text_length = 0;
};

// Matches the IDS view's visible row count. Read-only for now -- alias/
// group/watch/mute/notes editing is a separate, not-yet-implemented UI
// (phasing.md Phase 10.3).
constexpr size_t kIdentityCapacity = 12;
constexpr size_t kIdentityAliasLen = 20;
constexpr size_t kIdentityGroupLen = 12;

struct IdentitySummary {
  uint32_t capcode = 0;
  char alias[kIdentityAliasLen] = {};
  char group[kIdentityGroupLen] = {};
  bool watched = false;
  bool muted = false;
  uint32_t hit_count = 0;
  uint64_t last_seen_ms = 0;
};

struct Snapshot {
  DisplayMessage messages[kRecentMessageCapacity]{};
  size_t message_count = 0;  // valid entries, index 0 = newest
  IdentitySummary identities[kIdentityCapacity]{};
  size_t identity_count = 0;
  Stats decoder_stats{};
  uint32_t revision = 0;
  bool receiving = false;
};

enum class View : uint8_t { live, ids, signal, activity, archive, count };

enum class Action : uint8_t {
  none,
  settings_changed,
  exit,
};

void enter(const Settings& settings);
void leave();
void draw();
void update();
void set_live_snapshot(const Snapshot& snapshot);
Action handle_touch(int32_t x, int32_t y);
const Settings& settings();
uint8_t view();
bool active();
bool self_check();

}  // namespace orcsdr::pocsag
