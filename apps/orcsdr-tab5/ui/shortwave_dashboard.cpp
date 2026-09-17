#include "shortwave_dashboard.hpp"
#include "shortwave_dashboard_state.hpp"

#include "dashboard_audio_control.hpp"
#include "shortwave_model.hpp"
#include "spectrum_resample.hpp"
#include "text_editor.hpp"

#include <M5Unified.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace orcsdr::shortwave {
namespace {

constexpr uint16_t kPanel = 0x0841;
constexpr uint16_t kCyan = 0x2e7f;
constexpr uint16_t kGreen = 0x6fe8;
constexpr uint16_t kYellow = 0xff24;
constexpr uint16_t kMuted = 0x8c71;
constexpr uint16_t kGrid = 0x2945;
constexpr int kSpectrumX = 24;
constexpr int kSpectrumY = 340;
constexpr int kSpectrumW = 792;
constexpr int kSpectrumH = 145;
constexpr int kWaterfallY = 493;
constexpr int kWaterfallH = 91;
constexpr int kZoomY = 589;
constexpr int kTabsY = 630;
constexpr int kTabW = 256;
constexpr int kGainX = 872;
constexpr int kGainY = 488;
constexpr int kGainW = 350;

Snapshot g_snapshot{};
bool g_active = false;
DashboardState g_state{};
uint32_t g_saved_frequency = 7100000;
size_t g_hunt_band = 0;
char g_entry[12]{};
char g_pending_memory_label[64]{};
char g_pending_log_antenna[64]{};
char g_pending_log_notes[240]{};
EXT_RAM_BSS_ATTR uint16_t g_waterfall_row[kSpectrumW]{};

int spectrum_x_for_bin(size_t bin, size_t visible_bins) {
  return kSpectrumX + static_cast<int>(bin * (kSpectrumW - 1) /
                                       (visible_bins > 1 ? visible_bins - 1 : 1));
}

bool hit(int32_t x, int32_t y, int bx, int by, int bw, int bh) {
  return x >= bx && x < bx + bw && y >= by && y < by + bh;
}

void text(const char* value, int x, int y, uint16_t color = TFT_WHITE,
          int size = 2, textdatum_t datum = middle_center) {
  M5.Display.setTextDatum(datum);
  M5.Display.setTextSize(size);
  M5.Display.setTextColor(color);
  M5.Display.drawString(value, x, y);
}

void card(int x, int y, int w, int h) {
  M5.Display.fillRoundRect(x, y, w, h, 10, kPanel);
  M5.Display.drawRoundRect(x, y, w, h, 10, kCyan);
}

void button(int x, int y, int w, int h, const char* label, bool selected = false,
            bool enabled = true) {
  const uint16_t color = enabled ? (selected ? kGreen : kCyan) : TFT_DARKGREY;
  M5.Display.fillRoundRect(x, y, w, h, 8, selected ? 0x1264 : kPanel);
  M5.Display.drawRoundRect(x, y, w, h, 8, color);
  text(label, x + w / 2, y + h / 2, enabled ? TFT_WHITE : kMuted, 2);
}

const char* route_name(ReceiverRoute route) {
  switch (route) {
    case ReceiverRoute::direct_q: return "DIRECT Q SAMPLING";
    case ReceiverRoute::hf_upconverter: return "V4 HF UPCONVERTER";
    case ReceiverRoute::tuner: return "NORMAL TUNER";
    default: return "ROUTE UNKNOWN";
  }
}

void draw_frequency() {
  M5.Display.fillRect(120, 120, 565, 92, kPanel);
  char value[40];
  snprintf(value, sizeof(value), "%lu.%03lu.%03lu MHz",
           static_cast<unsigned long>(g_snapshot.frequency_hz / 1000000u),
           static_cast<unsigned long>((g_snapshot.frequency_hz / 1000u) % 1000u),
           static_cast<unsigned long>(g_snapshot.frequency_hz % 1000u));
  text(value, 402, 159, TFT_WHITE, 4);
  const BroadcastBand* sw_band = band_for(g_snapshot.frequency_hz);
  const ModeGuide guide = mode_guide_for(g_snapshot.frequency_hz);
  char context[64];
  snprintf(context, sizeof(context), "%s  |  %s %s",
           sw_band ? sw_band->label : region_label(region_for(g_snapshot.frequency_hz)),
           guide.likely_mode, guide.supported_now ? "READY" : "GUIDE");
  text(context, 402, 198, guide.supported_now ? kGreen : kYellow, 2);
}

void draw_status() {
  M5.Display.fillRect(862, 111, 374, 48, kPanel);
  text(route_name(g_snapshot.controls.route), 1049, 126, kGreen, 2);
  text(g_snapshot.device[0] ? g_snapshot.device : "NO RTL-SDR", 1049, 150,
       g_snapshot.driver_ready ? TFT_WHITE : TFT_ORANGE, 1);
  M5.Display.fillRect(35, 294, 760, 34, TFT_BLACK);
  char status[96];
  snprintf(status, sizeof(status), "%s  |  %+.1f dBFS  |  %lu kHz span%s",
           g_snapshot.running ? "LIVE IQ" : "WAITING",
           static_cast<double>(g_snapshot.relative_dbfs),
           static_cast<unsigned long>(g_snapshot.span_hz / 1000u),
           g_snapshot.clipping_percent > 0.1f ? "  |  CLIP" : "");
  text(status, 415, 311,
       g_snapshot.clipping_percent > 0.1f ? TFT_RED
                                         : g_snapshot.running ? kGreen : TFT_ORANGE,
       2);
}

void draw_quick_controls() {
  char value[40];
  snprintf(value, sizeof(value), "STEP %lu Hz",
           static_cast<unsigned long>(g_snapshot.step_hz));
  button(24, 246, 236, 56, value);
  snprintf(value, sizeof(value), "AM FILTER %.1f kHz",
           static_cast<double>(g_snapshot.filter_bandwidth_hz) / 1000.0);
  button(278, 246, 236, 56, value);
  button(532, 246, 284, 56,
         g_snapshot.sound_enabled ? "SOUND ON" : "SOUND OFF",
         g_snapshot.sound_enabled);
}

void draw_zoom_controls() {
  char span[32];
  snprintf(span, sizeof(span), "SPAN %lu kHz",
           static_cast<unsigned long>(g_snapshot.span_hz / 1000u));
  button(24, kZoomY, 160, 37, "ZOOM IN");
  text(span, 420, kZoomY + 19, kCyan, 2);
  button(656, kZoomY, 160, 37, "ZOOM OUT");
}

void draw_controls() {
  const auto tuner = receiver_controls::item(receiver_controls::Control::tuner_agc,
                                              g_snapshot.controls);
  const auto rtl = receiver_controls::item(receiver_controls::Control::rtl_agc,
                                            g_snapshot.controls);
  const auto boost = receiver_controls::item(receiver_controls::Control::audio_boost,
                                              g_snapshot.controls);
  const auto gain = receiver_controls::item(receiver_controls::Control::rf_gain,
                                             g_snapshot.controls);
  button(858, 176, 184, 68, "TUNER AGC", tuner.active,
         tuner.availability == receiver_controls::Availability::enabled);
  button(1052, 176, 184, 68, "RTL AGC", rtl.active,
         rtl.availability == receiver_controls::Availability::enabled);
  button(858, 258, 184, 68, "AUDIO BOOST", boost.active);
  button(1052, 258, 72, 68, "VOL -");
  button(1164, 258, 72, 68, "VOL +");
  char volume[24];
  snprintf(volume, sizeof(volume), "VOLUME %u", g_snapshot.controls.volume);
  text(volume, 1144, 345, TFT_WHITE, 2);

  M5.Display.fillRect(858, 378, 378, 184, kPanel);
  text("RF GAIN", 858, 382,
       gain.availability == receiver_controls::Availability::enabled ? kCyan : kMuted,
       2, top_left);
  if (gain.availability != receiver_controls::Availability::enabled) {
    text(gain.explanation, 1047, 440, kMuted, 2);
    text("Audio boost remains available", 1047, 474, TFT_LIGHTGREY, 1);
    return;
  }
  char gain_value[32];
  snprintf(gain_value, sizeof(gain_value), g_snapshot.controls.tuner_agc
                                                   ? "AUTO %.1f dB"
                                                   : "MANUAL %.1f dB",
           static_cast<double>(g_snapshot.controls.gain_tenth_db) / 10.0);
  text(gain_value, 1047, 426, g_snapshot.controls.tuner_agc ? kGreen : TFT_WHITE, 2);
  M5.Display.drawRoundRect(kGainX, kGainY, kGainW, 22, 10, kCyan);
  int position = 0;
  if (g_snapshot.gain_step_count > 1) {
    size_t nearest = 0;
    for (size_t i = 1; i < g_snapshot.gain_step_count; ++i)
      if (std::abs(g_snapshot.gain_steps_tenth_db[i] - g_snapshot.controls.gain_tenth_db) <
          std::abs(g_snapshot.gain_steps_tenth_db[nearest] - g_snapshot.controls.gain_tenth_db))
        nearest = i;
    position = static_cast<int>(nearest * kGainW / (g_snapshot.gain_step_count - 1));
  }
  M5.Display.fillCircle(kGainX + position, kGainY + 11, 12,
                        g_snapshot.controls.tuner_agc ? kMuted : kGreen);
  text("Tap TUNER AGC for auto; drag for manual", 1047, 535, TFT_LIGHTGREY, 1);
}

void draw_static() {
  M5.Display.clearScrollRect();
  M5.Display.fillScreen(TFT_BLACK);
  audio_header::draw_brand("SHORTWAVE EXPLORER");
  M5.Display.drawFastVLine(350, 18, 58, kCyan);
  text("SHORTWAVE", 390, 42, TFT_WHITE, 4, middle_left);
  audio_header::draw_battery(g_snapshot.battery_percent);
  audio_header::draw_home_button();
  audio_header::draw_mute_button(g_snapshot.sound_enabled);
  audio_header::draw_visualizer_button(g_snapshot.running);
  audio_header::draw_settings_button();
  M5.Display.drawFastHLine(20, 92, 1240, kGreen);

  card(24, 110, 792, 122);
  button(42, 128, 64, 72, "-");
  button(734, 128, 64, 72, "+");
  card(840, 110, 420, 474);
  M5.Display.drawRect(kSpectrumX, kSpectrumY, kSpectrumW, kSpectrumH, kGrid);
  for (int i = 1; i < 8; ++i)
    M5.Display.drawFastVLine(kSpectrumX + i * kSpectrumW / 8, kSpectrumY,
                            kSpectrumH, kGrid);
  for (int i = 1; i < 4; ++i)
    M5.Display.drawFastHLine(kSpectrumX, kSpectrumY + i * kSpectrumH / 4,
                            kSpectrumW, kGrid);
  M5.Display.drawRect(kSpectrumX, kWaterfallY, kSpectrumW, kWaterfallH, kCyan);
  M5.Display.setScrollRect(kSpectrumX + 1, kWaterfallY + 1, kSpectrumW - 2,
                           kWaterfallH - 2, TFT_BLACK);
  draw_zoom_controls();

  constexpr const char* tabs[] = {"LIVE", "ON AIR", "HUNT", "MEMORY", "LOGBOOK"};
  for (int i = 0; i < 5; ++i) {
    button(i * kTabW + 4, kTabsY + 4, kTabW - 8, 82, tabs[i],
           static_cast<int>(g_state.tab()) == i);
  }
}

void draw_page_title(const char* title, const char* subtitle) {
  M5.Display.clearScrollRect();
  M5.Display.fillRect(0, 93, 1280, kTabsY - 93, TFT_BLACK);
  text(title, 36, 126, TFT_WHITE, 4, middle_left);
  text(subtitle, 38, 166, kMuted, 2, middle_left);
}

void draw_storage_status() {
  const bool ready = g_snapshot.storage_status == StorageStatus::ready;
  text(ready ? "SD LIBRARY READY" : "SD LIBRARY UNAVAILABLE", 1220, 130,
       ready ? kGreen : TFT_ORANGE, 2, middle_right);
}

void draw_guide_card(int y) {
  const ModeGuide guide = mode_guide_for(g_snapshot.frequency_hz);
  card(36, y, 1208, 122);
  char heading[72];
  snprintf(heading, sizeof(heading), "%s  |  TRY %s",
           region_label(region_for(g_snapshot.frequency_hz)), guide.likely_mode);
  text(heading, 62, y + 30, guide.supported_now ? kGreen : kYellow, 3,
       middle_left);
  text(guide.reason, 62, y + 78, TFT_WHITE, 2, middle_left);
  if (!guide.supported_now)
    text("This mode is guidance only until its demodulator is implemented.",
         62, y + 103, kMuted, 1, middle_left);
}

void draw_on_air() {
  draw_page_title("ON AIR", "Local schedules are suggestions, never decoded identity");
  draw_storage_status();
  draw_guide_card(194);
  card(36, 340, 1208, 210);
  text("NO LOCAL SCHEDULE CATALOG LOADED", 640, 404, TFT_ORANGE, 3);
  text("Use LIVE to explore, or HUNT to scan an international broadcast band.",
       640, 452, TFT_WHITE, 2);
  text("A future catalog update can populate this page without changing the receiver.",
       640, 494, kMuted, 2);
}

void draw_hunt() {
  draw_page_title("HUNT", "Scan one broadcast band and keep the strongest candidates");
  const BroadcastBand* selected = band(g_hunt_band);
  char selection[64];
  snprintf(selection, sizeof(selection), "BAND  %s  %.3f-%.3f MHz",
           selected ? selected->label : "--",
           selected ? static_cast<double>(selected->min_hz) / 1000000.0 : 0.0,
           selected ? static_cast<double>(selected->max_hz) / 1000000.0 : 0.0);
  button(36, 190, 80, 58, "<");
  button(126, 190, 540, 58, selection, true);
  button(676, 190, 80, 58, ">");
  button(790, 190, 220, 58, g_snapshot.hunt_active ? "SCANNING" : "START",
         g_snapshot.hunt_active);
  button(1020, 190, 224, 58, "CANCEL", false, g_snapshot.hunt_active);
  char progress[48];
  snprintf(progress, sizeof(progress), "%u / %u", g_snapshot.hunt_step,
           g_snapshot.hunt_total);
  text(progress, 1170, 165, g_snapshot.hunt_active ? kGreen : kMuted, 2);
  if (!g_snapshot.hunt_candidate_count) {
    card(36, 280, 1208, 250);
    text(g_snapshot.hunt_active ? "LISTENING FOR PEAKS..." : "NO HUNT RESULTS YET",
         640, 370, g_snapshot.hunt_active ? kGreen : TFT_WHITE, 3);
    text("Select a band, start the scan, then tap a result to listen.",
         640, 420, kMuted, 2);
    return;
  }
  for (size_t i = 0; i < std::min<size_t>(g_snapshot.hunt_candidate_count, 6); ++i) {
    const Candidate& candidate = g_snapshot.hunt_candidates[i];
    char row[96];
    snprintf(row, sizeof(row), "%u.  %.3f MHz     %+.1f dBFS",
             static_cast<unsigned>(i + 1),
             static_cast<double>(candidate.frequency_hz) / 1000000.0,
             static_cast<double>(candidate.level_dbfs));
    button(36, 270 + static_cast<int>(i) * 52, 1208, 44, row);
  }
}

void draw_memory() {
  draw_page_title("MEMORY", "Saved frequencies live on the SD card");
  draw_storage_status();
  button(980, 170, 264, 58, "SAVE CURRENT", true,
         g_snapshot.storage_status == StorageStatus::ready);
  const size_t count = g_snapshot.memories ? g_snapshot.memories->size() : 0;
  if (!count) {
    card(36, 260, 1208, 270);
    text("NO SHORTWAVE MEMORIES", 640, 355, TFT_WHITE, 3);
    text("Tune a signal in LIVE, then save the current frequency here.",
         640, 410, kMuted, 2);
    return;
  }
  for (size_t i = 0; i < std::min<size_t>(count, 7); ++i) {
    const Memory* memory = g_snapshot.memories->at(i);
    if (!memory) continue;
    char row[128];
    snprintf(row, sizeof(row), "%u.  %.3f MHz  %-3s  %s",
             static_cast<unsigned>(i + 1),
             static_cast<double>(memory->frequency_hz) / 1000000.0,
             memory->mode, memory->station[0] ? memory->station : "Unlabeled signal");
    button(36, 250 + static_cast<int>(i) * 50, 1208, 42, row);
  }
}

void draw_logbook() {
  draw_page_title("LOGBOOK", "Reception history and community-friendly exports");
  draw_storage_status();
  button(720, 170, 250, 58, "LOG CURRENT", true,
         g_snapshot.storage_status == StorageStatus::ready);
  button(984, 170, 260, 58, "EXPORT CSV + ADIF", false,
         g_snapshot.storage_status == StorageStatus::ready);
  const size_t count = g_snapshot.logs ? g_snapshot.logs->size() : 0;
  if (!count) {
    card(36, 260, 1208, 270);
    text("NO RECEPTION LOGS", 640, 355, TFT_WHITE, 3);
    text("Log what you actually heard; station identity may remain unknown.",
         640, 410, kMuted, 2);
    return;
  }
  const size_t first = count > 7 ? count - 7 : 0;
  for (size_t row_index = 0; row_index < count - first; ++row_index) {
    const LogEntry* entry = g_snapshot.logs->at(first + row_index);
    if (!entry) continue;
    char row[144];
    snprintf(row, sizeof(row), "%.3f MHz  %-3s  %+.1f dBFS  %s",
             static_cast<double>(entry->frequency_hz) / 1000000.0,
             entry->mode, static_cast<double>(entry->signal_dbfs),
             entry->station[0] ? entry->station : "Unidentified reception");
    button(36, 250 + static_cast<int>(row_index) * 50, 1208, 42, row);
  }
}

void draw_keypad() {
  M5.Display.clearScrollRect();
  M5.Display.fillRect(0, 93, 1280, 627, TFT_BLACK);
  card(340, 135, 600, 470);
  text("ENTER SHORTWAVE FREQUENCY (MHz)", 640, 168, kCyan, 2);
  char field[24];
  snprintf(field, sizeof(field), "%s%s", g_entry, g_entry[0] ? " MHz" : "");
  M5.Display.fillRoundRect(380, 200, 520, 58, 8, TFT_NAVY);
  text(field[0] ? field : "0.024 - 30.000", 640, 229, TFT_WHITE, 3);
  static constexpr char keys[] = {'1','2','3','4','5','6','7','8','9','.','0','<'};
  for (int i = 0; i < 12; ++i) {
    char key[2] = {keys[i], 0};
    button(380 + (i % 3) * 174, 275 + (i / 3) * 60, 160, 50, key);
  }
  button(380, 525, 250, 55, "CANCEL");
  button(650, 525, 250, 55, "TUNE", true);
}

uint16_t waterfall_color(float level) {
  level = std::clamp(level, 0.0f, 1.0f);
  const uint8_t r = level < 0.55f ? 0 : static_cast<uint8_t>((level - 0.55f) * 566);
  const uint8_t g = level < 0.2f ? 0 : static_cast<uint8_t>(
      std::min(255.0f, (level - 0.2f) * 510));
  const uint8_t b = level < 0.65f ? static_cast<uint8_t>((0.65f - level) * 390) : 0;
  return M5.Display.color565(r, g, b);
}

}  // namespace

void enter(const Snapshot& snapshot) {
  g_snapshot = snapshot;
  g_saved_frequency = snapshot.frequency_hz;
  g_active = true;
  g_state.close_modal();
  g_state.select_tab(Tab::live);
  if (const BroadcastBand* current = band_for(snapshot.frequency_hz)) {
    for (size_t i = 0; i < band_count(); ++i)
      if (band(i) == current) g_hunt_band = i;
  }
  g_entry[0] = '\0';
  draw();
}

void leave() {
  M5.Display.clearScrollRect();
  g_active = false;
}

void draw() {
  if (!g_active) return;
  draw_static();
  if (g_state.modal() == Modal::frequency) {
    draw_keypad();
    return;
  }
  if (g_state.modal() != Modal::none) {
    text_editor::draw();
    return;
  }
  switch (g_state.tab()) {
    case Tab::live:
      draw_frequency();
      draw_status();
      draw_controls();
      draw_quick_controls();
      break;
    case Tab::on_air: draw_on_air(); break;
    case Tab::hunt: draw_hunt(); break;
    case Tab::memory: draw_memory(); break;
    case Tab::logbook: draw_logbook(); break;
  }
}

void update(const Snapshot& snapshot) {
  if (!g_active) return;
  const bool span_changed = snapshot.span_hz != g_snapshot.span_hz;
  const bool page_changed =
      snapshot.storage_status != g_snapshot.storage_status ||
      snapshot.memories != g_snapshot.memories || snapshot.logs != g_snapshot.logs ||
      snapshot.memory_count != g_snapshot.memory_count ||
      snapshot.log_count != g_snapshot.log_count ||
      snapshot.hunt_active != g_snapshot.hunt_active ||
      snapshot.hunt_step != g_snapshot.hunt_step ||
      snapshot.hunt_total != g_snapshot.hunt_total ||
      snapshot.hunt_candidate_count != g_snapshot.hunt_candidate_count;
  const bool controls_changed =
      snapshot.controls.route != g_snapshot.controls.route ||
      snapshot.controls.capabilities.rf_gain != g_snapshot.controls.capabilities.rf_gain ||
      snapshot.controls.capabilities.tuner_agc != g_snapshot.controls.capabilities.tuner_agc ||
      snapshot.controls.capabilities.rtl_agc != g_snapshot.controls.capabilities.rtl_agc ||
      snapshot.controls.tuner_agc != g_snapshot.controls.tuner_agc ||
      snapshot.controls.rtl_agc != g_snapshot.controls.rtl_agc ||
      snapshot.controls.audio_boost != g_snapshot.controls.audio_boost ||
      snapshot.controls.volume != g_snapshot.controls.volume ||
      snapshot.controls.gain_tenth_db != g_snapshot.controls.gain_tenth_db ||
      snapshot.gain_step_count != g_snapshot.gain_step_count;
  g_snapshot = snapshot;
  g_saved_frequency = snapshot.frequency_hz;
  if (!g_state.background_redraw_allowed()) return;
  if (g_state.tab() != Tab::live) {
    if (page_changed) {
      switch (g_state.tab()) {
        case Tab::on_air: draw_on_air(); break;
        case Tab::hunt: draw_hunt(); break;
        case Tab::memory: draw_memory(); break;
        case Tab::logbook: draw_logbook(); break;
        case Tab::live: break;
      }
    }
    return;
  }
  draw_frequency();
  draw_status();
  draw_quick_controls();
  if (span_changed) draw_zoom_controls();
  if (controls_changed) draw_controls();
}

void draw_spectrum(const float* levels, size_t first_bin, size_t visible_bins,
                   float floor) {
  if (!spectrum_active() || !levels || visible_bins < 2) return;
  M5.Display.startWrite();
  M5.Display.fillRect(kSpectrumX + 1, kSpectrumY + 1, kSpectrumW - 2,
                      kSpectrumH - 2, TFT_BLACK);
  int last_x = kSpectrumX;
  int last_y = kSpectrumY + kSpectrumH - 2;
  for (size_t i = 0; i < kSpectrumW; ++i) {
    const float level = spectrum::peak_for_pixel(
        levels, first_bin, visible_bins, i, kSpectrumW);
    const float normalized = std::clamp((level - floor) / 48.0f,
                                        0.0f, 1.0f);
    const int x = kSpectrumX + static_cast<int>(i);
    const int y = kSpectrumY + kSpectrumH - 2 -
                  static_cast<int>(normalized * (kSpectrumH - 4));
    if (i) M5.Display.drawLine(last_x, last_y, x, y, kGreen);
    last_x = x;
    last_y = y;
    g_waterfall_row[i] = waterfall_color(normalized);
  }
  const int center = kSpectrumX + kSpectrumW / 2;
  const int half_filter = std::clamp(static_cast<int>(
      static_cast<uint64_t>(g_snapshot.filter_bandwidth_hz) * kSpectrumW /
      (2u * (g_snapshot.span_hz ? g_snapshot.span_hz : 1u))), 3,
      kSpectrumW / 2 - 2);
  M5.Display.drawFastVLine(center, kSpectrumY, kSpectrumH, kCyan);
  M5.Display.drawFastVLine(center - half_filter, kSpectrumY, kSpectrumH, kYellow);
  M5.Display.drawFastVLine(center + half_filter, kSpectrumY, kSpectrumH, kYellow);
  M5.Display.scroll(0, -1);
  M5.Display.pushImage(kSpectrumX, kWaterfallY + kWaterfallH - 2, kSpectrumW, 1,
                       g_waterfall_row);
  M5.Display.endWrite();
}

Action handle_touch(int32_t x, int32_t y) {
  if (!g_active) return {};
  if (g_state.modal() != Modal::none && g_state.modal() != Modal::frequency) {
    const Modal modal = g_state.modal();
    const auto result = text_editor::handle_touch(x, y);
    if (result == text_editor::Result::cancelled) {
      g_state.close_modal();
      draw();
      return {};
    }
    if (result != text_editor::Result::accepted) return {};
    if (modal == Modal::memory_label) {
      snprintf(g_pending_memory_label, sizeof(g_pending_memory_label), "%s",
               text_editor::value());
      g_state.close_modal();
      draw();
      return {ActionKind::save_memory};
    }
    if (modal == Modal::log_antenna) {
      snprintf(g_pending_log_antenna, sizeof(g_pending_log_antenna), "%s",
               text_editor::value());
      g_state.open(Modal::log_notes);
      text_editor::begin("RECEPTION NOTES", "", sizeof(g_pending_log_notes) - 1,
                         false, "LOG");
      text_editor::draw();
      return {};
    }
    if (modal == Modal::log_notes) {
      snprintf(g_pending_log_notes, sizeof(g_pending_log_notes), "%s",
               text_editor::value());
      g_state.close_modal();
      draw();
      return {ActionKind::save_log};
    }
    return {};
  }
  if (g_state.modal() == Modal::frequency) {
    if (hit(x, y, 380, 525, 250, 55)) {
      g_state.close_modal();
      g_entry[0] = '\0';
      draw();
      return {};
    }
    if (hit(x, y, 650, 525, 250, 55)) {
      char* end = nullptr;
      const double mhz = strtod(g_entry, &end);
      if (end != g_entry && *end == '\0' && mhz >= 0.024 && mhz <= 30.0) {
        g_state.close_modal();
        const uint32_t hz = static_cast<uint32_t>(llround(mhz * 1000000.0));
        g_entry[0] = '\0';
        draw();
        return {ActionKind::tune_hz, static_cast<int32_t>(hz)};
      }
      return {};
    }
    static constexpr char keys[] = {'1','2','3','4','5','6','7','8','9','.','0','\b'};
    for (int i = 0; i < 12; ++i) {
      if (!hit(x, y, 380 + (i % 3) * 174, 275 + (i / 3) * 60, 160, 50)) continue;
      const size_t n = strlen(g_entry);
      if (keys[i] == '\b') {
        if (n) g_entry[n - 1] = '\0';
      } else if (n + 1 < sizeof(g_entry) &&
                 (keys[i] != '.' || strchr(g_entry, '.') == nullptr)) {
        g_entry[n] = keys[i];
        g_entry[n + 1] = '\0';
      }
      draw_keypad();
      return {};
    }
    return {};
  }
  if (audio_header::home_hit(x, y)) return {ActionKind::exit_home};
  if (audio_header::settings_hit(x, y)) return {ActionKind::open_settings};
  if (y >= kTabsY) {
    const int index = std::clamp<int32_t>(x / kTabW, 0, 4);
    g_state.select_tab(static_cast<Tab>(index));
    draw();
    return {};
  }

  if (g_state.tab() == Tab::on_air) return {};

  if (g_state.tab() == Tab::hunt) {
    if (hit(x, y, 36, 190, 80, 58)) {
      g_hunt_band = (g_hunt_band + band_count() - 1) % band_count();
      draw();
      return {};
    }
    if (hit(x, y, 676, 190, 80, 58)) {
      g_hunt_band = (g_hunt_band + 1) % band_count();
      draw();
      return {};
    }
    if (hit(x, y, 790, 190, 220, 58) && !g_snapshot.hunt_active)
      return {ActionKind::hunt_start, static_cast<int32_t>(g_hunt_band)};
    if (hit(x, y, 1020, 190, 224, 58) && g_snapshot.hunt_active)
      return {ActionKind::hunt_cancel};
    for (size_t i = 0; !g_snapshot.hunt_active &&
                       i < std::min<size_t>(g_snapshot.hunt_candidate_count, 6); ++i)
      if (hit(x, y, 36, 270 + static_cast<int>(i) * 52, 1208, 44))
        return {ActionKind::tune_hz,
                static_cast<int32_t>(g_snapshot.hunt_candidates[i].frequency_hz)};
    return {};
  }

  if (g_state.tab() == Tab::memory) {
    if (g_snapshot.storage_status == StorageStatus::ready &&
        hit(x, y, 980, 170, 264, 58)) {
      g_state.open(Modal::memory_label);
      text_editor::begin("MEMORY LABEL", "", sizeof(g_pending_memory_label) - 1,
                         false, "SAVE");
      text_editor::draw();
      return {};
    }
    const size_t count = g_snapshot.memories ? g_snapshot.memories->size() : 0;
    for (size_t i = 0; i < std::min<size_t>(count, 7); ++i) {
      const Memory* memory = g_snapshot.memories->at(i);
      if (memory && hit(x, y, 36, 250 + static_cast<int>(i) * 50, 1208, 42))
        return {ActionKind::tune_hz, static_cast<int32_t>(memory->frequency_hz)};
    }
    return {};
  }

  if (g_state.tab() == Tab::logbook) {
    if (g_snapshot.storage_status == StorageStatus::ready &&
        hit(x, y, 720, 170, 250, 58)) {
      g_state.open(Modal::log_antenna);
      text_editor::begin("ANTENNA USED", "", sizeof(g_pending_log_antenna) - 1,
                         false, "NEXT");
      text_editor::draw();
      return {};
    }
    if (g_snapshot.storage_status == StorageStatus::ready &&
        hit(x, y, 984, 170, 260, 58))
      return {ActionKind::export_log};
    return {};
  }

  if (hit(x, y, 42, 128, 64, 72)) return {ActionKind::step_down};
  if (hit(x, y, 734, 128, 64, 72)) return {ActionKind::step_up};
  if (hit(x, y, 120, 120, 565, 92)) {
    g_state.open(Modal::frequency);
    g_entry[0] = '\0';
    draw();
    return {};
  }
  if (hit(x, y, 24, 246, 236, 56)) return {ActionKind::step_cycle};
  if (hit(x, y, 278, 246, 236, 56)) return {ActionKind::filter_cycle};
  if (hit(x, y, 532, 246, 284, 56)) return {ActionKind::sound_toggle};
  if (hit(x, y, 24, kZoomY, 160, 37)) return {ActionKind::span_down};
  if (hit(x, y, 656, kZoomY, 160, 37)) return {ActionKind::span_up};
  if (hit(x, y, 858, 176, 184, 68) &&
      receiver_controls::action(receiver_controls::Control::tuner_agc,
                                g_snapshot.controls).kind !=
          receiver_controls::ActionKind::none)
    return {ActionKind::gain_auto};
  if (hit(x, y, 1052, 176, 184, 68) &&
      receiver_controls::action(receiver_controls::Control::rtl_agc,
                                g_snapshot.controls).kind !=
          receiver_controls::ActionKind::none)
    return {ActionKind::rtl_agc, !g_snapshot.controls.rtl_agc};
  if (hit(x, y, 858, 258, 184, 68))
    return {ActionKind::audio_boost, !g_snapshot.controls.audio_boost};
  if (hit(x, y, 1052, 258, 72, 68)) return {ActionKind::volume_down};
  if (hit(x, y, 1164, 258, 72, 68)) return {ActionKind::volume_up};
  if (hit(x, y, kSpectrumX, kSpectrumY, kSpectrumW, kSpectrumH) ||
      hit(x, y, kSpectrumX, kWaterfallY, kSpectrumW, kWaterfallH)) {
    const int64_t offset =
        static_cast<int64_t>(x - (kSpectrumX + kSpectrumW / 2)) *
        g_snapshot.span_hz / kSpectrumW;
    const int64_t selected = static_cast<int64_t>(g_snapshot.frequency_hz) + offset;
    return {ActionKind::tune_hz, static_cast<int32_t>(
        std::clamp<int64_t>(selected, 24000, 30000000))};
  }
  return {};
}

Action handle_gain_drag(int32_t x, int32_t y) {
  if (!g_active || g_state.tab() != Tab::live ||
      !hit(x, y, kGainX - 16, kGainY - 20, kGainW + 32, 62) ||
      g_snapshot.gain_step_count == 0 ||
      receiver_controls::item(receiver_controls::Control::rf_gain,
                              g_snapshot.controls).availability !=
          receiver_controls::Availability::enabled)
    return {};
  const int clamped = std::clamp<int32_t>(x, kGainX, kGainX + kGainW);
  const size_t index = static_cast<size_t>(clamped - kGainX) *
                       (g_snapshot.gain_step_count - 1) / kGainW;
  return {ActionKind::gain_tenth_db, g_snapshot.gain_steps_tenth_db[index]};
}

bool active() { return g_active; }
bool spectrum_active() { return g_active && g_state.spectrum_allowed(); }
uint32_t saved_frequency() { return g_saved_frequency; }
void note_tuned(uint32_t frequency_hz) { g_saved_frequency = frequency_hz; }
const char* pending_memory_label() { return g_pending_memory_label; }
const char* pending_log_antenna() { return g_pending_log_antenna; }
const char* pending_log_notes() { return g_pending_log_notes; }

bool dashboard_self_check() {
  const Snapshot saved = g_snapshot;
  const bool was_active = g_active;
  const Modal saved_modal = g_state.modal();
  const Tab saved_tab = g_state.tab();
  char saved_entry[sizeof(g_entry)];
  memcpy(saved_entry, g_entry, sizeof(g_entry));
  Snapshot test{};
  test.controls.route = ReceiverRoute::hf_upconverter;
  test.controls.capabilities = {true, true, true, false};
  test.gain_steps_tenth_db[0] = 0;
  test.gain_steps_tenth_db[1] = 297;
  test.gain_steps_tenth_db[2] = 496;
  test.gain_step_count = 3;
  g_snapshot = test;
  g_active = true;
  g_state.close_modal();
  g_state.select_tab(Tab::live);
  const bool ok = handle_touch(60, 150).kind == ActionKind::step_down &&
                   handle_touch(760, 150).kind == ActionKind::step_up &&
                   handle_touch(60, kZoomY + 10).kind == ActionKind::span_down &&
                   handle_touch(700, kZoomY + 10).kind == ActionKind::span_up &&
                   handle_touch(900, 200).kind == ActionKind::gain_auto &&
                   handle_gain_drag(kGainX + kGainW, kGainY).value == 496;
  g_snapshot.controls.route = ReceiverRoute::direct_q;
  const bool direct_q_ok = handle_touch(900, 200).kind == ActionKind::none &&
                           handle_gain_drag(kGainX, kGainY).kind == ActionKind::none &&
                           handle_touch(900, 280).kind == ActionKind::audio_boost;
  const bool spectrum_layout_ok =
      spectrum_x_for_bin(0, 4) == kSpectrumX &&
      spectrum_x_for_bin(3, 4) == kSpectrumX + kSpectrumW - 1 &&
      kWaterfallY > kSpectrumY + kSpectrumH &&
      kWaterfallY + kWaterfallH <= kZoomY && kZoomY + 37 < kTabsY;
  const float resolution_test[] = {-80.0f, -20.0f, -75.0f, -40.0f};
  const bool peak_pool_ok =
      spectrum::peak_for_pixel(resolution_test, 0, 4, 0, 2) == -20.0f &&
      spectrum::peak_for_pixel(resolution_test, 0, 4, 1, 2) == -40.0f;
  g_snapshot.frequency_hz = 7100000;
  g_snapshot.span_hz = 480000;
  const bool touch_tune_bounds_ok =
      handle_touch(kSpectrumX + kSpectrumW / 2, kSpectrumY).kind ==
          ActionKind::tune_hz &&
      handle_touch(kSpectrumX + kSpectrumW / 2, kSpectrumY).value == 7100000 &&
      handle_touch(kSpectrumX - 1, kSpectrumY).kind == ActionKind::none &&
      handle_touch(kSpectrumX, kSpectrumY - 1).kind == ActionKind::none &&
      handle_touch(kSpectrumX + kSpectrumW, kWaterfallY).kind == ActionKind::none &&
      handle_touch(kSpectrumX, kWaterfallY + kWaterfallH).kind == ActionKind::none &&
      handle_touch(830, 400).kind == ActionKind::none;
  g_snapshot = saved;
  g_active = was_active;
  g_state.open(saved_modal);
  g_state.select_tab(saved_tab);
  memcpy(g_entry, saved_entry, sizeof(g_entry));
  return ok && direct_q_ok && spectrum_layout_ok && peak_pool_ok &&
         touch_tune_bounds_ok &&
         kTabsY + 90 <= 720 && model_self_check() && receiver_controls::self_check();
}

}  // namespace orcsdr::shortwave
