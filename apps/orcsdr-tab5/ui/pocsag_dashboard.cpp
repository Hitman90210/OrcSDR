#include "pocsag_dashboard.hpp"

#include "dashboard_audio_control.hpp"
#include "orc_badge.hpp"

#include <M5Unified.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace orcsdr::pocsag {
namespace {

constexpr uint16_t kBg = TFT_BLACK;
constexpr uint16_t kPanel = 0x0861;
constexpr uint16_t kBorder = 0x2945;
constexpr uint16_t kCyan = 0x04ff;
constexpr uint16_t kGreen = 0x6fe8;
constexpr uint16_t kYellow = 0xffe0;
constexpr uint16_t kRed = 0xf800;
constexpr uint16_t kMuted = 0x9cf3;
constexpr int kHeaderH = 76;
constexpr int kTabsY = 646;
constexpr int kTabW = 256;

Settings g_settings;
View g_view = View::live;
bool g_active = false;
bool g_live = false;
Snapshot g_live_snapshot{};
uint32_t g_drawn_revision = 0;

void text(const char* value, int x, int y, uint16_t color = TFT_WHITE, int size = 2,
          textdatum_t datum = middle_center) {
  switch (size) {
    case 1: M5.Display.setFont(&fonts::DejaVu18); break;
    case 2: M5.Display.setFont(&fonts::DejaVu24); break;
    default: M5.Display.setFont(&fonts::DejaVu40); break;
  }
  M5.Display.setTextDatum(datum);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(color);
  M5.Display.drawString(value, x, y);
}

void card(int x, int y, int w, int h) {
  M5.Display.fillRoundRect(x, y, w, h, 12, kPanel);
  M5.Display.drawRoundRect(x, y, w, h, 12, kBorder);
}

bool hit(int32_t x, int32_t y, int bx, int by, int bw, int bh) {
  return x >= bx && x < bx + bw && y >= by && y < by + bh;
}

const char* lock_state_label(LockState state) {
  switch (state) {
    case LockState::locked: return "LOCKED";
    case LockState::searching: return "SEARCHING";
    case LockState::lost: return "LOST SYNC";
    default: return "NO SIGNAL";
  }
}

uint16_t lock_state_color(LockState state) {
  switch (state) {
    case LockState::locked: return kGreen;
    case LockState::lost: return kYellow;
    default: return kMuted;
  }
}

const char* message_type_label(MessageType type) {
  switch (type) {
    case MessageType::alpha: return "ALPHA";
    case MessageType::numeric: return "NUMERIC";
    case MessageType::tone_only: return "TONE";
    default: return "UNKNOWN";
  }
}

void tab_icon(int index, int x, int y, uint16_t color) {
  switch (index) {
    case 0:  // LIVE
      M5.Display.drawCircle(x, y, 13, color);
      M5.Display.drawCircle(x, y, 4, color);
      break;
    case 1:  // IDS
      M5.Display.fillCircle(x - 6, y - 4, 6, color);
      M5.Display.fillCircle(x + 6, y - 4, 6, color);
      M5.Display.drawFastHLine(x - 14, y + 10, 28, color);
      break;
    case 2:  // SIGNAL
      for (int i = 0; i < 4; ++i)
        M5.Display.fillRect(x - 16 + i * 9, y + 12 - (i + 1) * 6, 6, (i + 1) * 6, color);
      break;
    case 3:  // ACTIVITY
      M5.Display.drawLine(x - 16, y + 6, x - 6, y - 8, color);
      M5.Display.drawLine(x - 6, y - 8, x + 4, y + 2, color);
      M5.Display.drawLine(x + 4, y + 2, x + 16, y - 12, color);
      break;
    default:  // ARCHIVE
      M5.Display.drawRoundRect(x - 16, y - 10, 32, 22, 3, color);
      M5.Display.drawFastHLine(x - 16, y - 2, 32, color);
      break;
  }
}

void draw_header() {
  M5.Display.fillRect(0, 0, 1280, kHeaderH, kBg);
  M5.Display.drawFastHLine(20, kHeaderH - 1, 1240, kBorder);
  if (!badge::draw(12, 8, 58)) {
    M5.Display.drawRoundRect(12, 8, 58, 58, 8, kGreen);
    text("O", 41, 37, kGreen, 2);
  }
  text("OrcSDR", 82, 28, kGreen, 2, middle_left);
  text("POCSAG PAGER MONITOR", 82, 56, kCyan, 1, middle_left);
  M5.Display.drawFastVLine(340, 12, 52, kBorder);
  char freq[24];
  snprintf(freq, sizeof(freq), "%.4f MHz", g_settings.frequency_hz / 1000000.0);
  text(freq, 360, 36, TFT_WHITE, 2, middle_left);
  M5.Display.drawFastVLine(560, 12, 52, kBorder);
  const LockState lock = g_live_snapshot.decoder_stats.lock;
  M5.Display.fillRoundRect(580, 18, 150, 40, 8,
                            lock == LockState::locked ? TFT_DARKGREEN : TFT_DARKGREY);
  text(lock_state_label(lock), 655, 38, lock_state_color(lock), 1);
  M5.Display.drawFastVLine(750, 12, 52, kBorder);
  char msgs[24];
  snprintf(msgs, sizeof(msgs), "%u MSGS",
           static_cast<unsigned>(g_live_snapshot.decoder_stats.messages_decoded));
  text(msgs, 770, 36, TFT_WHITE, 2, middle_left);
  audio_header::draw_home_button();
  audio_header::draw_settings_button();
}

void draw_tabs() {
  static constexpr const char* labels[] = {"LIVE", "IDS", "SIGNAL", "ACTIVITY", "ARCHIVE"};
  M5.Display.fillRect(0, kTabsY, 1280, 74, kBg);
  M5.Display.drawFastHLine(0, kTabsY, 1280, kBorder);
  for (int i = 0; i < 5; ++i) {
    const bool selected = static_cast<int>(g_view) == i;
    if (selected) {
      M5.Display.fillRect(i * kTabW, kTabsY + 1, kTabW, 73, 0x08a4);
      M5.Display.fillRect(i * kTabW, kTabsY + 1, kTabW, 3, kCyan);
    }
    if (i) M5.Display.drawFastVLine(i * kTabW, kTabsY, 74, kBorder);
    const uint16_t color = selected ? kCyan : TFT_LIGHTGREY;
    tab_icon(i, i * kTabW + 68, kTabsY + 38, color);
    text(labels[i], i * kTabW + 96, kTabsY + 39, color, 1, middle_left);
  }
}

void draw_live() {
  card(20, kHeaderH + 12, 1240, 590);
  const Stats& stats = g_live_snapshot.decoder_stats;
  if (g_live_snapshot.message_count == 0) {
    text(g_live ? "WAITING FOR TRAFFIC..." : "NOT RECEIVING", 640, kHeaderH + 300, kMuted, 2);
    char status[80];
    snprintf(status, sizeof(status), "batches=%u codewords=%u corrected=%u uncorrectable=%u",
             static_cast<unsigned>(stats.batches_synced),
             static_cast<unsigned>(stats.codewords_total),
             static_cast<unsigned>(stats.codewords_corrected),
             static_cast<unsigned>(stats.codewords_uncorrectable));
    text(status, 640, kHeaderH + 340, kMuted, 1);
    return;
  }
  text("RECENT MESSAGES", 44, kHeaderH + 34, kCyan, 1, middle_left);
  const size_t visible = std::min<size_t>(g_live_snapshot.message_count, 12);
  for (size_t i = 0; i < visible; ++i) {
    const DisplayMessage& m = g_live_snapshot.messages[i];
    const int y = kHeaderH + 66 + static_cast<int>(i) * 44;
    char capcode[16];
    snprintf(capcode, sizeof(capcode), "%lu", static_cast<unsigned long>(m.capcode));
    text(capcode, 44, y, TFT_WHITE, 1, middle_left);
    text(message_type_label(m.type), 190, y,
         m.type == MessageType::alpha ? kGreen : m.type == MessageType::numeric ? kCyan : kMuted,
         1, middle_left);
    text(m.text_length ? m.text : "(no text)", 330, y, TFT_LIGHTGREY, 1, middle_left);
    if (m.uncorrectable_words) M5.Display.fillCircle(1230, y, 6, kRed);
    else if (m.corrected_bits) M5.Display.fillCircle(1230, y, 6, kYellow);
    else M5.Display.fillCircle(1230, y, 6, kGreen);
  }
}

void draw_placeholder(const char* label) {
  card(20, kHeaderH + 12, 1240, 590);
  text(label, 640, kHeaderH + 290, kMuted, 2);
  text("Not yet implemented in this build", 640, kHeaderH + 330, kMuted, 1);
}

void draw_ids() {
  card(20, kHeaderH + 12, 1240, 590);
  if (g_live_snapshot.identity_count == 0) {
    text("NO CAPCODES OBSERVED YET", 640, kHeaderH + 300, kMuted, 2);
    return;
  }
  text("CAPCODE DIRECTORY", 44, kHeaderH + 34, kCyan, 1, middle_left);
  text("(read-only -- alias/group/watch/mute editing not yet implemented)",
       44, kHeaderH + 570, kMuted, 1, middle_left);
  const size_t visible = std::min<size_t>(g_live_snapshot.identity_count, 12);
  for (size_t i = 0; i < visible; ++i) {
    const IdentitySummary& id = g_live_snapshot.identities[i];
    const int y = kHeaderH + 66 + static_cast<int>(i) * 44;
    if (id.watched) M5.Display.fillCircle(44, y, 5, kYellow);
    char capcode[16];
    snprintf(capcode, sizeof(capcode), "%lu", static_cast<unsigned long>(id.capcode));
    text(capcode, 64, y, TFT_WHITE, 1, middle_left);
    text(id.alias[0] ? id.alias : "UNKNOWN", 230, y,
         id.alias[0] ? TFT_LIGHTGREY : kMuted, 1, middle_left);
    text(id.group[0] ? id.group : "-", 480, y, kMuted, 1, middle_left);
    char hits[16];
    snprintf(hits, sizeof(hits), "%lu HITS", static_cast<unsigned long>(id.hit_count));
    text(hits, 640, y, kGreen, 1, middle_left);
    if (id.muted) text("MUTED", 900, y, kRed, 1, middle_left);
  }
}

void redraw_content() {
  M5.Display.fillRect(0, kHeaderH, 1280, kTabsY - kHeaderH, kBg);
  switch (g_view) {
    case View::live: draw_live(); break;
    case View::ids: draw_ids(); break;
    case View::signal: draw_placeholder("SIGNAL - RF AND DECODE DIAGNOSTICS"); break;
    case View::activity: draw_placeholder("ACTIVITY - TRAFFIC STATISTICS"); break;
    case View::archive: draw_placeholder("ARCHIVE - SAVED MESSAGE LOG"); break;
    default: break;
  }
}

void redraw() {
  M5.Display.fillScreen(kBg);
  draw_header();
  redraw_content();
  draw_tabs();
}

}  // namespace

void enter(const Settings& settings_value) {
  g_settings = settings_value;
  g_view = View::live;
  g_active = true;
  redraw();
}

void leave() {
  g_active = false;
  M5.Display.setFont(nullptr);
}

void draw() {
  if (g_active) redraw();
}

void update() {
  if (!g_active || !g_live || g_drawn_revision == g_live_snapshot.revision) return;
  static uint32_t last_draw_ms = 0;
  if (millis() - last_draw_ms < 1000) return;
  last_draw_ms = millis();
  g_drawn_revision = g_live_snapshot.revision;
  M5.Display.fillRect(340, 12, 460, 52, kBg);
  M5.Display.fillRect(750, 12, 200, 52, kBg);
  char freq[24];
  snprintf(freq, sizeof(freq), "%.4f MHz", g_settings.frequency_hz / 1000000.0);
  text(freq, 360, 36, TFT_WHITE, 2, middle_left);
  const LockState lock = g_live_snapshot.decoder_stats.lock;
  M5.Display.fillRoundRect(580, 18, 150, 40, 8,
                            lock == LockState::locked ? TFT_DARKGREEN : TFT_DARKGREY);
  text(lock_state_label(lock), 655, 38, lock_state_color(lock), 1);
  char msgs[24];
  snprintf(msgs, sizeof(msgs), "%u MSGS",
           static_cast<unsigned>(g_live_snapshot.decoder_stats.messages_decoded));
  text(msgs, 770, 36, TFT_WHITE, 2, middle_left);
  if (g_view == View::live) {
    M5.Display.fillRect(20, kHeaderH + 12, 1240, 590, kBg);
    draw_live();
  } else if (g_view == View::ids) {
    M5.Display.fillRect(20, kHeaderH + 12, 1240, 590, kBg);
    draw_ids();
  }
}

void set_live_snapshot(const Snapshot& snapshot) {
  g_live_snapshot = snapshot;
  g_live = true;
}

Action handle_touch(int32_t x, int32_t y) {
  if (!g_active) return Action::none;
  if (audio_header::home_hit(x, y)) return Action::exit;
  if (audio_header::settings_hit(x, y)) return Action::none;
  if (y >= kTabsY) {
    g_view = static_cast<View>(constrain(x / kTabW, 0, 4));
    redraw();
    return Action::none;
  }
  return Action::none;
}

const Settings& settings() { return g_settings; }

uint8_t view() { return static_cast<uint8_t>(g_view); }

bool active() { return g_active; }

bool self_check() {
  Settings settings_value;
  settings_value.frequency_hz = 152007500;
  enter(settings_value);
  const bool entered = active() && view() == static_cast<uint8_t>(View::live);
  Snapshot snapshot;
  snapshot.revision = 1;
  snapshot.receiving = true;
  snapshot.message_count = 1;
  snapshot.messages[0].capcode = 1234567;
  snapshot.messages[0].type = MessageType::alpha;
  std::strncpy(snapshot.messages[0].text, "TEST", sizeof(snapshot.messages[0].text) - 1);
  snapshot.messages[0].text_length = 4;
  set_live_snapshot(snapshot);
  const bool touch_selects_tab = handle_touch(2 * kTabW + 10, kTabsY + 10) == Action::none &&
                                  view() == static_cast<uint8_t>(View::signal);
  const bool exits = handle_touch(1050, 18) == Action::exit;
  leave();
  return entered && touch_selects_tab && exits && !active();
}

}  // namespace orcsdr::pocsag
