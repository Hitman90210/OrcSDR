#include "pocsag_dashboard.hpp"

#include "dashboard_audio_control.hpp"
#include "orc_badge.hpp"

#include <M5Unified.h>
#include <esp_heap_caps.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <new>

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
// Content area between the header and tab bar, with a 12px top margin and a
// 12px bottom margin so cards never overlap the tab strip.
constexpr int kContentH = kTabsY - (kHeaderH + 12) - 12;

Settings g_settings;
View g_view = View::live;
bool g_active = false;
bool g_live = false;
uint32_t g_drawn_revision = 0;

// Lazily PSRAM-allocated rather than a plain internal-DRAM global: this
// Snapshot (message/identity arrays plus the decoder Stats) is a few KB,
// and a prior revision's plain global -- combined with similar globals in
// main.cpp -- pushed static BSS just far enough to starve ESP-IDF's own
// early internal/DMA heap-pool reservation, producing a boot-time abort
// before setup() even runs (confirmed by flashing an unmodified baseline,
// which boots cleanly on the same hardware).
//
// A prior revision of this function "fixed" that by falling back to a
// `static Snapshot fallback{};` local on allocation failure -- but a
// function-local static of non-trivial type still reserves its full
// sizeof(Snapshot) in internal-DRAM BSS at link time, regardless of
// whether the fallback path is ever taken. That reintroduced the exact
// problem this function exists to avoid. Falling back to an internal-heap
// allocation (still dynamic, so it costs nothing unless actually used) is
// the only fallback that doesn't have this defect; PSRAM is abundant
// enough (32 MB) that neither allocation should realistically fail.
Snapshot& live_snapshot() {
  static Snapshot* storage = nullptr;
  if (!storage) {
    void* memory = heap_caps_malloc(sizeof(Snapshot), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!memory) memory = heap_caps_malloc(sizeof(Snapshot), MALLOC_CAP_8BIT);
    if (memory) storage = new (memory) Snapshot();
  }
  return *storage;
}

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

// Frequency/lock-badge/message-count region, redrawn both on full header
// entry and on every live update (~1/s) without touching the Home/Settings
// buttons either side of it.
void draw_header_live_values() {
  M5.Display.fillRect(340, 12, 460, 52, kBg);
  M5.Display.fillRect(750, 12, 200, 52, kBg);
  char freq[24];
  snprintf(freq, sizeof(freq), "%.4f MHz", g_settings.frequency_hz / 1000000.0);
  text(freq, 360, 36, TFT_WHITE, 2, middle_left);
  const Snapshot& snapshot = live_snapshot();
  if (snapshot.scanning) {
    char scan_freq[24];
    snprintf(scan_freq, sizeof(scan_freq), "%.4f MHz", snapshot.scan_frequency_hz / 1000000.0);
    text(scan_freq, 360, 36, kYellow, 2, middle_left);
    M5.Display.fillRoundRect(580, 18, 150, 40, 8, TFT_DARKGREY);
    char scan_label[16];
    snprintf(scan_label, sizeof(scan_label), "%u/%u", static_cast<unsigned>(snapshot.scan_index + 1),
             static_cast<unsigned>(snapshot.scan_count));
    text("SCANNING", 655, 30, kYellow, 1);
    text(scan_label, 655, 46, TFT_WHITE, 1);
    return;
  }
  const LockState lock = snapshot.decoder_stats.lock;
  M5.Display.fillRoundRect(580, 18, 150, 40, 8,
                            lock == LockState::locked ? TFT_DARKGREEN : TFT_DARKGREY);
  text(lock_state_label(lock), 655, 38, lock_state_color(lock), 1);
  char msgs[24];
  snprintf(msgs, sizeof(msgs), "%u MSGS",
           static_cast<unsigned>(snapshot.decoder_stats.messages_decoded));
  text(msgs, 770, 36, TFT_WHITE, 2, middle_left);
}

// Fits the gap between the shared Home button (ends x=1098) and the shared
// Settings gear (starts x=1217) -- see dashboard_audio_control.cpp's own
// kHomeX/kHomeW/kSettingsX. Do not widen this without rechecking that gap.
constexpr int kScanButtonX = 1108, kScanButtonY = 12, kScanButtonW = 100, kScanButtonH = 52;

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
  M5.Display.drawFastVLine(560, 12, 52, kBorder);
  M5.Display.drawFastVLine(750, 12, 52, kBorder);
  draw_header_live_values();
  M5.Display.fillRoundRect(kScanButtonX, kScanButtonY, kScanButtonW, kScanButtonH, 8,
                            TFT_DARKCYAN);
  text("SCAN", kScanButtonX + kScanButtonW / 2, kScanButtonY + kScanButtonH / 2, TFT_WHITE, 1);
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
  card(20, kHeaderH + 12, 1240, kContentH);
  const Stats& stats = live_snapshot().decoder_stats;
  if (live_snapshot().message_count == 0) {
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
  const size_t visible = std::min<size_t>(live_snapshot().message_count, 12);
  for (size_t i = 0; i < visible; ++i) {
    const DisplayMessage& m = live_snapshot().messages[i];
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

void draw_signal() {
  const Stats& stats = live_snapshot().decoder_stats;

  // Left: decode status readout.
  card(20, kHeaderH + 12, 500, kContentH);
  text("DECODE STATUS", 44, kHeaderH + 34, kCyan, 1, middle_left);
  int y = kHeaderH + 74;
  char line[64];
  text("SYNC STATE", 44, y, kMuted, 1, middle_left);
  text(lock_state_label(stats.lock), 400, y, lock_state_color(stats.lock), 1, middle_right);
  y += 36;
  text("BAUD", 44, y, kMuted, 1, middle_left);
  snprintf(line, sizeof(line), "%u bps", stats.detected_baud);
  text(line, 400, y, TFT_WHITE, 1, middle_right);
  y += 36;
  text("POLARITY", 44, y, kMuted, 1, middle_left);
  text(stats.inverted ? "INVERTED" : "NORMAL", 400, y, TFT_WHITE, 1, middle_right);
  y += 36;
  text("FSK DEVIATION", 44, y, kMuted, 1, middle_left);
  snprintf(line, sizeof(line), "%+.1f Hz", static_cast<double>(stats.fsk_deviation_hz));
  text(line, 400, y, TFT_WHITE, 1, middle_right);
  y += 48;
  text("BATCHES SYNCED", 44, y, kMuted, 1, middle_left);
  snprintf(line, sizeof(line), "%lu", static_cast<unsigned long>(stats.batches_synced));
  text(line, 400, y, TFT_WHITE, 1, middle_right);
  y += 36;
  text("SYNC LOSSES", 44, y, kMuted, 1, middle_left);
  snprintf(line, sizeof(line), "%lu", static_cast<unsigned long>(stats.sync_losses));
  text(line, 400, y, TFT_WHITE, 1, middle_right);
  y += 48;
  text("CODEWORDS", 44, y, kMuted, 1, middle_left);
  snprintf(line, sizeof(line), "%lu", static_cast<unsigned long>(stats.codewords_total));
  text(line, 400, y, TFT_WHITE, 1, middle_right);
  y += 36;
  text("VALID", 44, y, kMuted, 1, middle_left);
  snprintf(line, sizeof(line), "%lu", static_cast<unsigned long>(stats.codewords_valid));
  text(line, 400, y, kGreen, 1, middle_right);
  y += 36;
  text("CORRECTED", 44, y, kMuted, 1, middle_left);
  snprintf(line, sizeof(line), "%lu bits=%lu", static_cast<unsigned long>(stats.codewords_corrected),
           static_cast<unsigned long>(stats.corrected_bit_count));
  text(line, 400, y, kYellow, 1, middle_right);
  y += 36;
  text("UNCORRECTABLE", 44, y, kMuted, 1, middle_left);
  snprintf(line, sizeof(line), "%lu", static_cast<unsigned long>(stats.codewords_uncorrectable));
  text(line, 400, y, kRed, 1, middle_right);
  y += 36;
  text("PARITY FAILURES", 44, y, kMuted, 1, middle_left);
  snprintf(line, sizeof(line), "%lu", static_cast<unsigned long>(stats.parity_failures));
  text(line, 400, y, kRed, 1, middle_right);

  // Right, top: 2-FSK soft-decision symbol plot -- an actual measured
  // instrument (recent discriminator samples), not a decorative waveform.
  constexpr int kPlotX = 540, kPlotY = kHeaderH + 12, kPlotW = 720, kPlotH = 280;
  card(kPlotX, kPlotY, kPlotW, kPlotH);
  text("2-FSK SYMBOL PLOT (SOFT DECISIONS)", kPlotX + 20, kPlotY + 24, kCyan, 1, middle_left);
  const int plot_x0 = kPlotX + 20, plot_y0 = kPlotY + 50;
  const int plot_w = kPlotW - 40, plot_h = kPlotH - 70;
  const int mid_y = plot_y0 + plot_h / 2;
  M5.Display.drawFastHLine(plot_x0, mid_y, plot_w, kBorder);
  if (stats.soft_symbol_count > 0) {
    const size_t count = std::min(stats.soft_symbol_count, Stats::kSoftSymbolCapacity);
    for (size_t i = 0; i < count; ++i) {
      const size_t index = (stats.soft_symbol_write + Stats::kSoftSymbolCapacity - count + i) %
                            Stats::kSoftSymbolCapacity;
      const float sample = std::clamp(stats.soft_symbols[index], -1.0f, 1.0f);
      const int px = plot_x0 + static_cast<int>(i * plot_w / Stats::kSoftSymbolCapacity);
      const int py = mid_y - static_cast<int>(sample * (plot_h / 2 - 4));
      M5.Display.fillCircle(px, py, 2, sample >= 0.0f ? kGreen : kCyan);
    }
  } else {
    text("NO SAMPLES YET", plot_x0 + plot_w / 2, mid_y, kMuted, 1);
  }

  // Right, bottom: FEC-quality strip over the most recent codewords.
  const int strip_x = kPlotX, strip_y = kPlotY + kPlotH + 20;
  const int strip_w = kPlotW, strip_h = kContentH - kPlotH - 20;
  card(strip_x, strip_y, strip_w, strip_h);
  text("FEC QUALITY (RECENT CODEWORDS)", strip_x + 20, strip_y + 24, kCyan, 1, middle_left);
  const int bars_x0 = strip_x + 20, bars_y0 = strip_y + 50;
  const int bars_w = strip_w - 40, bars_h = strip_h - 70;
  if (stats.fec_history_count > 0) {
    const size_t count = std::min(stats.fec_history_count, Stats::kFecHistoryCapacity);
    const int bar_w = std::max(1, bars_w / static_cast<int>(Stats::kFecHistoryCapacity));
    for (size_t i = 0; i < count; ++i) {
      const size_t index = (stats.fec_history_write + Stats::kFecHistoryCapacity - count + i) %
                            Stats::kFecHistoryCapacity;
      const uint8_t outcome = stats.fec_history[index];
      const uint16_t color = outcome == 0 ? kGreen : outcome == 1 ? kYellow : kRed;
      M5.Display.fillRect(bars_x0 + static_cast<int>(i) * bar_w, bars_y0, bar_w - 1, bars_h, color);
    }
  } else {
    text("NO CODEWORDS YET", bars_x0 + bars_w / 2, bars_y0 + bars_h / 2, kMuted, 1);
  }
}

void kpi_card(int x, int y, int w, int h, const char* label, const char* value,
              uint16_t value_color) {
  card(x, y, w, h);
  text(label, x + 16, y + 22, kMuted, 1, middle_left);
  text(value, x + w / 2, y + h / 2 + 10, value_color, 2);
}

void draw_activity() {
  const Stats& stats = live_snapshot().decoder_stats;

  const int kpi_y = kHeaderH + 12, kpi_h = 90, kpi_gap = 16;
  const int kpi_w = (1240 - kpi_gap * 3) / 4;
  char value[24];

  snprintf(value, sizeof(value), "%lu", static_cast<unsigned long>(stats.messages_decoded));
  kpi_card(20, kpi_y, kpi_w, kpi_h, "TOTAL MESSAGES", value, TFT_WHITE);

  snprintf(value, sizeof(value), "%u", static_cast<unsigned>(live_snapshot().identity_count));
  kpi_card(20 + (kpi_w + kpi_gap), kpi_y, kpi_w, kpi_h, "ACTIVE IDS", value, TFT_WHITE);

  const uint32_t total_codewords = stats.codewords_total;
  const float valid_pct = total_codewords
                               ? 100.0f * static_cast<float>(stats.codewords_valid) /
                                     static_cast<float>(total_codewords)
                               : 0.0f;
  snprintf(value, sizeof(value), "%.1f%%", static_cast<double>(valid_pct));
  kpi_card(20 + 2 * (kpi_w + kpi_gap), kpi_y, kpi_w, kpi_h, "VALID CODEWORDS", value, kGreen);

  snprintf(value, sizeof(value), "%lu", static_cast<unsigned long>(stats.codewords_uncorrectable));
  kpi_card(20 + 3 * (kpi_w + kpi_gap), kpi_y, kpi_w, kpi_h, "UNCORRECTABLE", value, kRed);

  const int panel_y = kpi_y + kpi_h + 16;
  const int panel_h = kHeaderH + 12 + kContentH - panel_y;
  const int panel_gap = 16;
  const int panel_w = (1240 - panel_gap) / 2;

  // Left: message-type distribution.
  card(20, panel_y, panel_w, panel_h);
  text("MESSAGE TYPE DISTRIBUTION", 44, panel_y + 24, kCyan, 1, middle_left);
  const uint32_t type_counts[3] = {stats.messages_alpha, stats.messages_numeric,
                                    stats.messages_tone_only};
  const char* type_labels[3] = {"ALPHA", "NUMERIC", "TONE ONLY"};
  const uint16_t type_colors[3] = {kGreen, kCyan, kMuted};
  uint32_t type_max = 1;
  for (uint32_t count : type_counts) type_max = std::max(type_max, count);
  const int bar_x0 = 160, bar_max_w = panel_w - 200;
  for (int i = 0; i < 3; ++i) {
    const int y = panel_y + 66 + i * 56;
    text(type_labels[i], 44, y, TFT_WHITE, 1, middle_left);
    const int bar_w = static_cast<int>(static_cast<uint64_t>(type_counts[i]) * bar_max_w / type_max);
    M5.Display.fillRect(bar_x0, y - 12, std::max(bar_w, 2), 24, type_colors[i]);
    snprintf(value, sizeof(value), "%lu", static_cast<unsigned long>(type_counts[i]));
    text(value, bar_x0 + bar_max_w + 16, y, TFT_WHITE, 1, middle_left);
  }
  if (stats.messages_decoded == 0)
    text("NO MESSAGES YET", 44 + panel_w / 2 - 44, panel_y + panel_h / 2 + 40, kMuted, 1);

  // Right: top CAPCODEs by hit count (sorted from the bounded identity
  // snapshot -- a small, cheap sort of at most kIdentityCapacity entries).
  const int right_x = 20 + panel_w + panel_gap;
  card(right_x, panel_y, panel_w, panel_h);
  text("TOP CAPCODES", right_x + 24, panel_y + 24, kCyan, 1, middle_left);
  if (live_snapshot().identity_count == 0) {
    text("NO CAPCODES OBSERVED YET", right_x + panel_w / 2, panel_y + panel_h / 2, kMuted, 1);
    return;
  }
  size_t order[kIdentityCapacity];
  const size_t identity_count = std::min(live_snapshot().identity_count, kIdentityCapacity);
  for (size_t i = 0; i < identity_count; ++i) order[i] = i;
  std::sort(order, order + identity_count, [](size_t a, size_t b) {
    return live_snapshot().identities[a].hit_count > live_snapshot().identities[b].hit_count;
  });
  const size_t top_count = std::min<size_t>(identity_count, 8);
  uint32_t top_max = 1;
  for (size_t i = 0; i < top_count; ++i)
    top_max = std::max(top_max, live_snapshot().identities[order[i]].hit_count);
  const int top_bar_x0 = right_x + 200, top_bar_max_w = panel_w - 240;
  for (size_t i = 0; i < top_count; ++i) {
    const IdentitySummary& id = live_snapshot().identities[order[i]];
    const int y = panel_y + 66 + static_cast<int>(i) * 44;
    char capcode[16];
    snprintf(capcode, sizeof(capcode), "%lu", static_cast<unsigned long>(id.capcode));
    text(id.alias[0] ? id.alias : capcode, right_x + 24, y, TFT_WHITE, 1, middle_left);
    const int bar_w =
        static_cast<int>(static_cast<uint64_t>(id.hit_count) * top_bar_max_w / top_max);
    M5.Display.fillRect(top_bar_x0, y - 10, std::max(bar_w, 2), 20, kGreen);
    snprintf(value, sizeof(value), "%lu", static_cast<unsigned long>(id.hit_count));
    text(value, top_bar_x0 + top_bar_max_w + 16, y, TFT_WHITE, 1, middle_left);
  }
}

void draw_placeholder(const char* label) {
  card(20, kHeaderH + 12, 1240, kContentH);
  text(label, 640, kHeaderH + 290, kMuted, 2);
  text("Not yet implemented in this build", 640, kHeaderH + 330, kMuted, 1);
}

void draw_ids() {
  card(20, kHeaderH + 12, 1240, kContentH);
  if (live_snapshot().identity_count == 0) {
    text("NO CAPCODES OBSERVED YET", 640, kHeaderH + 300, kMuted, 2);
    return;
  }
  text("CAPCODE DIRECTORY", 44, kHeaderH + 34, kCyan, 1, middle_left);
  text("(read-only -- alias/group/watch/mute editing not yet implemented)",
       44, kHeaderH + 12 + kContentH - 24, kMuted, 1, middle_left);
  const size_t visible = std::min<size_t>(live_snapshot().identity_count, 12);
  for (size_t i = 0; i < visible; ++i) {
    const IdentitySummary& id = live_snapshot().identities[i];
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
    case View::signal: draw_signal(); break;
    case View::activity: draw_activity(); break;
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
  if (!g_active || !g_live || g_drawn_revision == live_snapshot().revision) return;
  static uint32_t last_draw_ms = 0;
  if (millis() - last_draw_ms < 1000) return;
  last_draw_ms = millis();
  g_drawn_revision = live_snapshot().revision;
  draw_header_live_values();
  if (g_view == View::live) {
    M5.Display.fillRect(20, kHeaderH + 12, 1240, kContentH, kBg);
    draw_live();
  } else if (g_view == View::ids) {
    M5.Display.fillRect(20, kHeaderH + 12, 1240, kContentH, kBg);
    draw_ids();
  } else if (g_view == View::signal) {
    M5.Display.fillRect(20, kHeaderH + 12, 1240, kContentH, kBg);
    draw_signal();
  } else if (g_view == View::activity) {
    M5.Display.fillRect(20, kHeaderH + 12, 1240, kContentH, kBg);
    draw_activity();
  }
}

void set_live_snapshot(const Snapshot& snapshot) {
  live_snapshot() = snapshot;
  g_live = true;
}

Action handle_touch(int32_t x, int32_t y) {
  if (!g_active) return Action::none;
  if (audio_header::home_hit(x, y)) return Action::exit;
  if (audio_header::settings_hit(x, y)) return Action::none;
  if (!live_snapshot().scanning &&
      hit(x, y, kScanButtonX, kScanButtonY, kScanButtonW, kScanButtonH))
    return Action::scan_requested;
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
