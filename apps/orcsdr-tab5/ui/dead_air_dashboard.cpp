#include "dead_air_dashboard.hpp"
#include "dead_air_info.hpp"
#include "dashboard_audio_control.hpp"
#include "screen_controller.hpp"
#include <M5Unified.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <new>
#include <utility>
#include <chrono>
#if defined(ESP_PLATFORM)
#include <esp_timer.h>
#endif
#include "acars_dirty_rects.hpp"
#include "dead_air_sweep.hpp"
#include "dead_air_format.hpp"
#include "dead_air_palette.hpp"

namespace orcsdr::dead_air {
namespace {
constexpr uint16_t kBg = TFT_BLACK, kPanel = 0x0841, kEdge = 0x2945,
  kGreen = 0x07e0, kCyan = 0x2e7f, kViolet = 0xa15f,
  kMuted = 0x8c71, kYellow = 0xffe0, kRed = 0xf800;
constexpr int kTabY = 649, kTabX = 20, kTabWidth = 198, kTabGap = 12, kTabHeight = 52;
constexpr int kBodyY = 114, kBodyBottom = 635;
constexpr int kModeX0 = 40, kModeY0 = 290, kModeDx = 95, kModeDy = 60, kModeW = 86;
constexpr int kStepY = 452;
constexpr int kSweepY = 530;
EventModel* g_events = nullptr;
Snapshot* g_snapshot = nullptr;  // heap, not static: internal .data/.bss below the DMA-pool margin aborts boot
Page g_page = Page::overview;
bool g_active = false;

// Body panels are drawn into an off-screen canvas and pushed once per update, so a refresh never shows a
// cleared frame (that was the flicker). Drawing code keeps absolute screen coordinates; `oy` shifts them
// into the canvas. If the canvas cannot be allocated, drawing falls back to the display directly.
struct Gfx {
  lgfx::LovyanGFX* t = nullptr;
  int oy = 0;
  lgfx::LovyanGFX& d() const { return t ? *t : static_cast<lgfx::LovyanGFX&>(M5.Display); }
  template <class D> void setTextDatum(D v) { d().setTextDatum(v); }
  void setTextSize(uint8_t s) { d().setTextSize(s); }
  void setTextColor(uint16_t c) { d().setTextColor(c); }
  void drawString(const char* s, int x, int y) { d().drawString(s, x, y - oy); }
  void fillRoundRect(int x, int y, int w, int h, int r, uint16_t c) { d().fillRoundRect(x, y - oy, w, h, r, c); }
  void drawRoundRect(int x, int y, int w, int h, int r, uint16_t c) { d().drawRoundRect(x, y - oy, w, h, r, c); }
  void drawFastHLine(int x, int y, int w, uint16_t c) { d().drawFastHLine(x, y - oy, w, c); }
  void drawLine(int x0, int y0, int x1, int y1, uint16_t c) { d().drawLine(x0, y0 - oy, x1, y1 - oy, c); }
  void fillRect(int x, int y, int w, int h, uint16_t c) { d().fillRect(x, y - oy, w, h, c); }
  void drawFastVLine(int x, int y, int h, uint16_t c) { d().drawFastVLine(x, y - oy, h, c); }
  void pushImage(int x, int y, int w, int h, const uint16_t* p) { d().pushImage(x, y - oy, w, h, p); }
};
Gfx g;
// Two sprites, as in the ACARS dashboard: a frame is composed in g_canvas and compared with g_front (what the
// panel shows). Only differing tiles are pushed, because a whole-frame push is a slow strided write on the rotated
// panel and shows as a right-to-left sweep. A page change draws straight to the display with the native
// primitives; the composed sprite then becomes the reference for later partial pushes.
M5Canvas* g_canvas = nullptr;
M5Canvas* g_front = nullptr;
bool g_front_valid = false;
constexpr int kBodyHeight = 522;  // kBodyBottom - kBodyY + 1
constexpr size_t kMaxPushRects = 96;
constexpr int kWholeFramePercent = 55;

M5Canvas* make_canvas() {
  M5Canvas* c = new (std::nothrow) M5Canvas(&M5.Display);
  if (!c) return nullptr;
  c->setColorDepth(16);
  c->setPsram(true);
  if (!c->createSprite(1280, kBodyHeight)) { delete c; return nullptr; }
  return c;
}
M5Canvas* body_canvas() {
  if (g_canvas) return g_canvas;
  g_canvas = make_canvas();
  if (g_canvas && !g_front) g_front = make_canvas();  // optional: without it every refresh draws directly
  g_front_valid = false;
  return g_canvas;
}
void release_canvas() {
  for (M5Canvas** c : {&g_canvas, &g_front}) {
    if (*c) { (*c)->deleteSprite(); delete *c; *c = nullptr; }
  }
  g_front_valid = false;
}

bool hit(int x, int y, int rx, int ry, int rw, int rh) {
  return x >= rx && y >= ry && x < rx + rw && y < ry + rh;
}
void label(const char* value, int x, int y, uint16_t color = TFT_WHITE, uint8_t scale = 2) {
  g.setTextDatum(middle_left);
  g.setTextSize(scale);
  g.setTextColor(color);
  g.drawString(value, x, y);
}
void box(int x, int y, int w, int h, uint16_t border = kEdge) {
  g.fillRoundRect(x, y, w, h, 8, kPanel);
  g.drawRoundRect(x, y, w, h, 8, border);
}
void heading(const char* text, int x, int y) { label(text, x, y, kCyan, 2); }
void divider(int x, int y, int width) { g.drawFastHLine(x, y, width, kEdge); }
uint32_t now_us() {
#if defined(ESP_PLATFORM)
  return static_cast<uint32_t>(esp_timer_get_time());
#else
  return static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::microseconds>(
                                   std::chrono::steady_clock::now().time_since_epoch()).count());
#endif
}
// ---- Ectoplasm spectrum and waterfall --------------------------------------------------------------------------
// Live data is drawn straight onto the panel, the way the Home, LoRa and Shortwave dashboards do it: the trace erases
// its previous polyline and draws the new one, and the waterfall scrolls with the panel's own scroll plus one new row.
// The off-screen page holds only the static frame (boxes, grid, legend), which does not change between spectrum
// frames, so the tile diff never repaints this area. Colour is only a map over measured data (dead_air_palette.hpp).
constexpr int kPlotX = 373, kPlotY = 166, kPlotW = 862, kPlotH = 130;
constexpr int kWfX = 373, kWfY = 306, kWfW = 862, kWfH = 150;
constexpr int kLegendY = 464;
constexpr float kTrailDecayDbPerS = 12.0f;
constexpr uint32_t kWaterfallRowMs = 100;  // about 10 waterfall rows per second, whatever the trace rate
struct Scope {
  uint16_t lut[viz::kLutSize];
  viz::Trail trail;
  int16_t prev_x[kTracePoints], prev_y[kTracePoints], prev_trail_y[kTracePoints];
  size_t prev_n = 0;
  uint16_t row[kWfW];  // native RGB565, pushed as rgb565_t
  uint32_t last_row_ms = 0, last_frame_ms = 0, last_epoch = 0;
  bool have_epoch = false;
  bool was_valid = false;
  int16_t a_prev_half[kBins];  // audio envelope as last drawn (half height in pixels)
  size_t a_prev_n = 0;
  int a_state = -1;            // 0 = notice shown, 1 = live envelope drawn
  uint32_t a_text_ms = 0;
};
Scope* g_scope = nullptr;
PaintStats g_stats{};

uint16_t plot_bg() { return viz::rgb565(6, 3, 18); }
uint16_t plot_grid() { return viz::rgb565(34, 18, 66); }

void scope_alloc() {
  if (g_scope) return;
  g_scope = new (std::nothrow) Scope();
  if (g_scope) viz::build_lut(g_scope->lut);
}
void scope_release() {
  delete g_scope;
  g_scope = nullptr;
}

int level_y(float dbfs, int top, int height) {
  return top + height - 1 - static_cast<int>(viz::normalize(dbfs) * static_cast<float>(height - 2));
}
void draw_plot_grid(lgfx::LovyanGFX& d) {
  const int x0 = kPlotX + 1, w = kPlotW - 2;
  for (int db = -20; db >= -100; db -= 20) d.drawFastHLine(x0, level_y(static_cast<float>(db), kPlotY, kPlotH), w, plot_grid());
}

// Static page elements (drawn with the rest of the page, never per spectrum frame).
void draw_trace() {
  g.fillRect(kPlotX, kPlotY, kPlotW, kPlotH, plot_bg());
  for (int db = -20; db >= -100; db -= 20)
    g.drawFastHLine(kPlotX, level_y(static_cast<float>(db), kPlotY, kPlotH), kPlotW, plot_grid());
  g.drawRoundRect(kPlotX, kPlotY, kPlotW, kPlotH, 0, plot_grid());
  label("-20", kPlotX + 6, level_y(-20.0f, kPlotY, kPlotH) + 10, kMuted, 2);
  label("-60", kPlotX + 6, level_y(-60.0f, kPlotY, kPlotH) + 10, kMuted, 2);
  label("-100", kPlotX + 6, level_y(-100.0f, kPlotY, kPlotH) - 10, kMuted, 2);
}
void draw_waterfall() {
  g.fillRect(kWfX, kWfY, kWfW, kWfH, plot_bg());
  g.drawRoundRect(kWfX, kWfY, kWfW, kWfH, 0, plot_grid());
}
void draw_legend() {
  if (g_scope) {
    for (int x = 0; x < kWfW; ++x)
      g.drawFastVLine(kWfX + x, kLegendY, 10, g_scope->lut[x * (viz::kLutSize - 1) / (kWfW - 1)]);
  }
  static const char* const kTicks[] = {"-110", "-85", "-60", "-35", "-10 dBFS"};
  for (int i = 0; i < 5; ++i) {
    const int len = static_cast<int>(std::strlen(kTicks[i]));
    int x = kWfX + i * (kWfW - 1) / 4 - len * 6;
    x = std::max(kWfX, std::min(x, kWfX + kWfW - len * 12));
    label(kTicks[i], x, 489, kMuted, 2);
  }
}

// Erases the live trace (not the waterfall history) and forgets it.
void scope_erase_trace() {
  if (!g_scope) return;
  Scope& s = *g_scope;
  M5.Display.startWrite();
  M5.Display.fillRect(kPlotX + 1, kPlotY + 1, kPlotW - 2, kPlotH - 2, plot_bg());
  draw_plot_grid(M5.Display);
  M5.Display.endWrite();
  s.prev_n = 0;
  s.trail.reset();
}

void waterfall_push_row(Scope& s) {
  M5.Display.scroll(0, -1);
  M5.Display.pushImage(kWfX + 1, kWfY + kWfH - 3, kWfW - 2, 1, reinterpret_cast<const lgfx::rgb565_t*>(s.row));
}

void button(const char* text, int x, int y, int w, uint16_t accent, bool enabled) {
  box(x, y, w, 52, enabled ? accent : kEdge);
  label(text, x + 16, y + 26, enabled ? TFT_WHITE : kMuted);
}
void reading(const char* name, const Reading& value, int x, int y, const char* format) {
  char output[32] = "--";
  const Status state = reading_status(value);
  if (state == Status::current) snprintf(output, sizeof(output), format, static_cast<double>(value.value));
  label(name, x, y, kMuted);
  label(output, x + 230, y, state == Status::current ? TFT_WHITE : kMuted, 2);
  if (state == Status::stale) label("STALE", x + 400, y, kYellow, 2);
}
const char* recorder_text(RecorderState state) {
  switch (state) {
    case RecorderState::starting: return "STARTING";
    case RecorderState::active: return "SAVING TO SD";
    case RecorderState::failed: return "SD FAILED";
    case RecorderState::stopped: return "STOPPED";
    default: return "NO SD / OFF";
  }
}
uint16_t recorder_color(RecorderState state) {
  switch (state) {
    case RecorderState::active: return kGreen;
    case RecorderState::starting: return kYellow;
    case RecorderState::failed: return kRed;
    default: return kMuted;
  }
}
void nav() {
  static constexpr const char* tabs[] = {"Overview", "Spectrum", "Audio", "Environment", "Events", "Review"};
  M5.Display.fillRect(0, 641, 1280, 79, kBg);
  for (uint8_t i = 0; i < static_cast<uint8_t>(Page::count); ++i) {
    const int x = kTabX + i * (kTabWidth + kTabGap);
    box(x, kTabY, kTabWidth, kTabHeight, g_page == static_cast<Page>(i) ? kGreen : kEdge);
    label(tabs[i], x + 17, kTabY + 26,
          g_page == static_cast<Page>(i) ? kGreen : TFT_WHITE, 2);
  }
}
void sanitize_snapshot() {
  // A disconnected module may leave cached readings. Never render those live.
  if (!g_snapshot->environment_sensor_attached) {
    g_snapshot->temperature_c = {};
    g_snapshot->humidity_rh = {};
    g_snapshot->pressure_hpa = {};
  }
  if (!g_snapshot->receiver_ready) {
    g_snapshot->rf_level_dbfs = {};
    g_snapshot->spectrum_valid = false;
  }
  if (!g_snapshot->audio_available) {
    g_snapshot->audio_level_dbfs = {};
    g_snapshot->audio_envelope_valid = false;
  }
}
// The (i) button and its "about this page" popup, the same idea as the FT8 dashboard's. The popup covers the body
// (below the header, above the tab bar); while it is open the body is not repainted underneath it.
bool g_info_open = false;
size_t g_info_page = 0;
constexpr int kInfoBtnX = 374, kInfoBtnY = 17, kInfoBtnW = 44, kInfoBtnH = 44;
constexpr int kInfoPanelX = 160, kInfoPanelY = 108, kInfoPanelW = 960, kInfoPanelH = 520;
constexpr size_t kInfoLinesPerPage = 12;
constexpr int kInfoPrevX = 180, kInfoNextX = 350, kInfoCloseX = 950, kInfoBtnRowY = 560, kInfoBtnRowW = 150;
info::Topic info_topic() {
  const size_t p = static_cast<size_t>(g_page);
  return p < static_cast<size_t>(info::Topic::count) ? static_cast<info::Topic>(p) : info::Topic::overview;
}
size_t info_page_count() {
  const info::Page p = info::page(info_topic());
  return (p.line_count + kInfoLinesPerPage - 1) / kInfoLinesPerPage;
}
void draw_info_button() {
  const int cx = kInfoBtnX + kInfoBtnW / 2, cy = kInfoBtnY + kInfoBtnH / 2;
  const uint16_t c = g_info_open ? kGreen : kCyan;
  M5.Display.fillCircle(cx, cy, 21, g_info_open ? kEdge : kPanel);
  M5.Display.drawCircle(cx, cy, 21, c);
  M5.Display.drawCircle(cx, cy, 20, c);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextSize(3);
  M5.Display.setTextColor(c);
  M5.Display.drawString("i", cx, cy);
}
void draw_info_panel() {
  g.t = &M5.Display;
  g.oy = 0;
  const info::Page p = info::page(info_topic());
  const size_t pages = info_page_count();
  if (g_info_page >= pages) g_info_page = pages ? pages - 1 : 0;
  box(kInfoPanelX, kInfoPanelY, kInfoPanelW, kInfoPanelH, kCyan);
  char heading[64];
  snprintf(heading, sizeof(heading), "ABOUT THIS PAGE  -  %s", p.title);
  label(heading, kInfoPanelX + 22, kInfoPanelY + 28, kCyan, 2);
  divider(kInfoPanelX + 20, kInfoPanelY + 52, kInfoPanelW - 40);
  for (size_t i = 0; i < kInfoLinesPerPage; ++i) {
    const size_t index = g_info_page * kInfoLinesPerPage + i;
    if (index >= p.line_count) break;
    if (p.lines[index][0]) label(p.lines[index], kInfoPanelX + 24, kInfoPanelY + 78 + static_cast<int>(i) * 30, TFT_WHITE, 2);
  }
  char page_text[24];
  snprintf(page_text, sizeof(page_text), "PAGE %u/%u", static_cast<unsigned>(g_info_page + 1), static_cast<unsigned>(pages ? pages : 1));
  label(page_text, 560, kInfoBtnRowY + 26, kMuted, 2);
  button("PREV", kInfoPrevX, kInfoBtnRowY, kInfoBtnRowW, kCyan, g_info_page > 0);
  button("NEXT", kInfoNextX, kInfoBtnRowY, kInfoBtnRowW, kCyan, g_info_page + 1 < pages);
  button("CLOSE", kInfoCloseX, kInfoBtnRowY, kInfoBtnRowW, kCyan, true);
}

// Always-visible state of the optional microphone. Blank while it is off.
void mic_banner() {
  M5.Display.fillRect(420, 12, 446, 54, kBg);
  const uint8_t m = g_snapshot->mic_state;
  if (!m) return;
  const uint16_t color = m == 1 ? kRed : m == 2 ? kYellow : kRed;
  M5.Display.fillRoundRect(420, 12, 446, 54, 8, kPanel);
  M5.Display.drawRoundRect(420, 12, 446, 54, 8, color);
  if (m == 1) M5.Display.fillCircle(446, 39, 9, color);
  label(m == 1 ? "MIC LISTENING" : m == 2 ? "MIC PAUSED: SPEAKER ON" : "MIC FAILED", m == 1 ? 466 : 440, 39, color, 2);
}
void chrome() {
  M5.Display.setFont(nullptr);
  M5.Display.fillScreen(kBg);
  audio_header::draw_brand("DEAD AIR");
  audio_header::draw_battery(g_snapshot->battery_percent);
  audio_header::draw_home_button();
  audio_header::draw_mute_button(g_snapshot->sound_enabled);
  audio_header::draw_visualizer_button(false);
  audio_header::draw_settings_button();
  mic_banner();
  draw_info_button();
  nav();
}
void overview() {
  box(20, 120, 612, 252);
  heading("INVESTIGATION", 40, 143);
  label(g_snapshot->recording ? "RECORDING" : "READY", 40, 190,
        g_snapshot->recording ? kRed : kGreen, 3);
  char buffer[52];
  snprintf(buffer, sizeof(buffer), "ELAPSED %02lu:%02lu:%02lu",
    static_cast<unsigned long>(g_snapshot->session_elapsed_s / 3600),
    static_cast<unsigned long>(g_snapshot->session_elapsed_s / 60 % 60),
    static_cast<unsigned long>(g_snapshot->session_elapsed_s % 60));
  label(buffer, 40, 240, TFT_WHITE);
  if (g_snapshot->receiver_ready) {
    snprintf(buffer, sizeof(buffer), "%.3f MHz  %s", g_snapshot->frequency_hz / 1e6,
             g_snapshot->demod);
    label(buffer, 40, 291, kCyan, 3);
  } else label("RECEIVER UNAVAILABLE", 40, 291, kYellow, 2);
  char journal[64];
  snprintf(journal, sizeof(journal), "EVENT LOG: %s", recorder_text(g_snapshot->recorder_state));
  label(journal, 40, 343, recorder_color(g_snapshot->recorder_state), 2);
  box(646, 120, 614, 252);
  heading("LIVE OBSERVATIONS", 668, 143);
  reading("RF (dBFS)", g_snapshot->rf_level_dbfs, 670, 192, "%.1f");
  reading("AUDIO (dBFS)", g_snapshot->audio_level_dbfs, 670, 247, "%.1f");
  divider(666, 283, 565);
  label("Sensitivity", 670, 321, kMuted);
  label(g_events && g_events->sensitivity() == Sensitivity::quiet ? "QUIET" :
    g_events && g_events->sensitivity() == Sensitivity::sensitive ? "SENSITIVE" : "STANDARD",
    935, 321, kGreen);
  box(20, 390, 612, 241);
  heading("LATEST EVENTS", 40, 413);
  if (!g_events || !g_events->count()) label("No recorded observations yet", 40, 483, kMuted);
  else for (size_t i = 0; i < std::min<size_t>(4, g_events->count()); ++i) {
    const Event* e = g_events->newest(i); if (!e) continue;
    char row[96];
    format_event_row(*e, row, sizeof(row));
    label(row, 40, 462 + i * 38, i == 0 ? kGreen : TFT_WHITE, 2);
  }
  box(646, 390, 614, 241);
  heading("ENV III / OPTIONAL", 668, 413);
  label(g_snapshot->environment_sensor_attached ? "MODULE DETECTED" : "MODULE NOT CONNECTED",
        668, 464, g_snapshot->environment_sensor_attached ? kGreen : kMuted, 2);
  reading("TEMP (C)", g_snapshot->temperature_c, 668, 505, "%.1f");
  reading("RH (%)", g_snapshot->humidity_rh, 668, 550, "%.1f");
  reading("PRESSURE (hPa)", g_snapshot->pressure_hpa, 668, 594, "%.1f");
}
void spectrum() {
  box(20, 120, 312, 511);
  heading("TUNING", 40, 144);
  char hz[48];
  if (g_snapshot->receiver_ready) snprintf(hz, sizeof(hz), "%.3f MHz", g_snapshot->frequency_hz / 1e6);
  else snprintf(hz, sizeof(hz), "NO RECEIVER");
  label(hz, 40, 201, kGreen, 3);
  divider(40, 248, 271);
  label("MODE (RECEIVER DEMODULATOR)", 40, 270, kMuted, 2);
  for (int i = 0; i < kModeCount; ++i) {
    const bool selected = g_snapshot->receiver_ready && std::strcmp(g_snapshot->demod, kModeNames[i]) == 0;
    const int x = kModeX0 + (i % 3) * kModeDx, y = kModeY0 + (i / 3) * kModeDy;
    box(x, y, kModeW, 52, selected ? kGreen : g_snapshot->receiver_ready ? kCyan : kEdge);
    label(kModeNames[i], x + 14, y + 26, selected ? kGreen : g_snapshot->receiver_ready ? TFT_WHITE : kMuted);
  }
  label("TUNE (BAND STEP)", 40, 432, kMuted, 2);
  button("- STEP", 40, 452, 128, kCyan, g_snapshot->receiver_ready && !g_snapshot->sweeping);
  button("+ STEP", 183, 452, 128, kCyan, g_snapshot->receiver_ready && !g_snapshot->sweeping);
  label(g_snapshot->receiver_settled ? "SETTLED" : "SETTLING...", 40, 535,
        g_snapshot->receiver_settled ? kGreen : kYellow, 2);
  label("Single channel,", 40, 580, kMuted, 2);
  label("not a sweep", 40, 605, kMuted, 2);
  box(346, 120, 914, 511);
  heading("RF SPECTRUM  /  dBFS", 366, 144);
  if (g_snapshot->receiver_ready) label(hz, 986, 144, kGreen, 2);
  if (!(g_snapshot->receiver_ready && g_snapshot->spectrum_valid))
    label(g_snapshot->receiver_ready ? "SETTLING" : "NO LIVE SPECTRUM", 640, 144, kYellow, 2);
  draw_trace();
  draw_waterfall();
  draw_legend();
  heading("SWEEP  /  GENERATED SCHEDULE", 366, 509);
  const bool ready = g_snapshot->receiver_ready;
  const bool run = g_snapshot->sweeping;
  button(run ? "STOP SWEEP" : "START SWEEP", 366, kSweepY, 230, run ? kRed : kGreen, ready);
  button(sweep_preset(g_snapshot->sweep_preset).name, 606, kSweepY, 300, kCyan, ready && !run);
  char dwell[32];
  snprintf(dwell, sizeof(dwell), "DWELL %u ms", static_cast<unsigned>(g_snapshot->sweep_dwell_ms));
  button(dwell, 916, kSweepY, 320, kCyan, ready && !run);
  char info[96];
  if (run)
    snprintf(info, sizeof(info), "HOP %lu  UNOBSERVED %.1f s  RF DETECTOR OFF",
             static_cast<unsigned long>(g_snapshot->sweep_hops), g_snapshot->sweep_unobserved_ms / 1000.0);
  else
    snprintf(info, sizeof(info), "One tuner, one channel at a time. Not simultaneous coverage.");
  label(info, 366, 608, run ? kYellow : kMuted, 2);
}
constexpr int kAuX = 42, kAuY = 171, kAuW = 1194, kAuH = 180;
constexpr int kAuStep = 6;  // pixels the plot rolls per 80 ms level reading
constexpr float kAudioFloorDb = -70.0f;  // envelope scale: -70..0 dBFS maps to 0..full half height

int audio_half(float norm) {
  const int max_half = kAuH / 2 - 3;
  return static_cast<int>(std::clamp(norm, 0.0f, 1.0f) * static_cast<float>(max_half));
}
void draw_audio_grid(lgfx::LovyanGFX& d, int oy) {
  const int cy = kAuY + kAuH / 2 - oy;
  d.drawFastHLine(kAuX + 1, cy, kAuW - 2, plot_grid());
  for (int db = -20; db >= -60; db -= 20) {
    const int h = audio_half((static_cast<float>(db) - kAudioFloorDb) / -kAudioFloorDb);
    d.drawFastHLine(kAuX + 1, cy - h, kAuW - 2, plot_grid());
    d.drawFastHLine(kAuX + 1, cy + h, kAuW - 2, plot_grid());
  }
}
void audio() {
  box(20, 120, 1240, 251);
  heading("DEMODULATED AUDIO  /  PEAK ENVELOPE", 40, 143);
  label("0 = full scale", 900, 143, kMuted, 2);
  g.fillRect(kAuX, kAuY, kAuW, kAuH, plot_bg());
  draw_audio_grid(g.d(), g.oy);
  g.drawRoundRect(kAuX, kAuY, kAuW, kAuH, 0, plot_grid());
  label("-20", kAuX + 6, kAuY + kAuH / 2 - audio_half(50.0f / 70.0f) - 12, kMuted, 2);
  label("-40", kAuX + 6, kAuY + kAuH / 2 - audio_half(30.0f / 70.0f) - 12, kMuted, 2);
  box(20, 388, 773, 243);
  heading("AUDIO ANALYSIS", 40, 415);
  label("RMS LEVEL", 40, 470, kMuted);
  label("PEAK", 40, 515, kMuted);
  label("BASELINE", 40, 560, kMuted);
  label("Level meter only. No speech claims.", 40, 606, kMuted, 2);
  box(809, 388, 451, 243);
  heading("SOURCE", 829, 415);
  label(g_snapshot->receiver_ready ? g_snapshot->demod : "NO AUDIO SOURCE", 829, 462, TFT_WHITE, 3);
  label("Clip replay: not implemented", 829, 606, kMuted, 2);
}
void environment() {
  box(20, 120, 610, 511);
  heading("ENV III / OPTIONAL SENSOR", 40, 144);
  label(g_snapshot->environment_sensor_attached ? "CONNECTED" : "UNAVAILABLE",
        40, 208, g_snapshot->environment_sensor_attached ? kGreen : kMuted, 3);
  reading("TEMPERATURE (C)", g_snapshot->temperature_c, 40, 280, "%.1f");
  reading("HUMIDITY (%)", g_snapshot->humidity_rh, 40, 355, "%.1f");
  reading("PRESSURE (hPa)", g_snapshot->pressure_hpa, 40, 430, "%.1f");
  label("Core RF investigation works without ENV III.", 40, 548, kMuted, 2);
  box(644, 120, 616, 511);
  heading("SENSOR INTEGRITY", 666, 144);
  label("No derived score without validated baselines", 666, 201, kMuted, 2);
  reading("DEVICE MOTION (g)", g_snapshot->motion_g, 666, 271, "%.3f");
  label("This describes Tab5 motion, NOT room movement.", 666, 331, kMuted, 2);
  label("Temperature / humidity / pressure", 666, 426, kCyan, 2);
  label("shown only from current valid sensor samples", 666, 476, kMuted, 2);
}
bool event_has_clip(uint32_t sequence) {
  for (uint8_t i = 0; i < g_snapshot->clip_event_seq_count && i < 16; ++i)
    if (g_snapshot->clip_event_seqs[i] == sequence) return true;
  return false;
}
void events() {
  box(20, 120, 767, 511);
  heading("EVENT LOG / LAST 7", 40, 144);
  if (!g_events || !g_events->count()) {
    label("NO EVENTS RECORDED", 43, 266, kMuted, 3);
  } else {
    const size_t rows = std::min<size_t>(7, g_events->count());
    for (size_t i = 0; i < rows; ++i) {
      const Event* e = g_events->newest(i); if (!e) continue;
      char line[96];
      format_event_row(*e, line, sizeof(line));
      const int y = 206 + static_cast<int>(i) * 55;
      label(line, 40, y, e->source == Source::manual ? kViolet : e->source == Source::sweep ? kMuted : TFT_WHITE, 2);
      if (event_has_clip(e->sequence)) label("CLIP", 690, y, kGreen, 2);
      if (i != rows - 1) divider(40, 233 + static_cast<int>(i) * 55, 718);
    }
  }
  box(801, 120, 459, 511);
  heading("DETECTION", 822, 144);
  label("AUTO: RF RISE + AUDIO RISE", 822, 205, kCyan, 2);
  const Sensitivity sens = g_events ? g_events->sensitivity() : Sensitivity::standard;
  char threshold[64];
  snprintf(threshold, sizeof(threshold), "Threshold +%.0f dB", rf_rise_threshold_db(sens));
  label(threshold, 822, 258, TFT_WHITE, 3);
  const bool rf_ready = g_events && g_events->baseline_ready();
  label(rf_ready ? "RF BASELINE READY" : "RF BASELINE WARMING", 822, 310, rf_ready ? kGreen : kYellow, 2);
  label(!g_snapshot->audio_available ? "AUDIO: NOT FLOWING" : g_snapshot->audio_baseline_ready ? "AUDIO BASELINE READY" : "AUDIO BASELINE WARMING",
        822, 342, !g_snapshot->audio_available ? kMuted : g_snapshot->audio_baseline_ready ? kGreen : kYellow, 2);
  const char* names[3] = {"QUIET", "NORMAL", "HIGH"};
  const int xs[3] = {818, 956, 1094}, ws[3] = {135, 135, 145};
  for (int i = 0; i < 3; ++i) {
    const bool selected = static_cast<int>(sens) == i;  // Sensitivity: quiet, standard, sensitive
    box(xs[i], 382, ws[i], 52, selected ? kGreen : kCyan);
    label(names[i], xs[i] + 16, 408, selected ? kGreen : TFT_WHITE);
  }
  button("MARK EVENT", 818, 505, 420, kRed, g_events != nullptr);
}
constexpr int kClipBtnX = 40, kClipBtnY = 366, kClipBtnW = 300, kClipBtnH = 52;
constexpr int kMicBtnX = 40, kMicBtnY = 548, kMicBtnW = 300, kMicBtnH = 52;
constexpr int kRowX = 517, kRowY0 = 172, kRowDy = 44, kRowW = 723, kRowH = 40;
constexpr int kVerifyBtnX = 517, kVerifyBtnY = 466, kVerifyBtnW = 260, kVerifyBtnH = 52;
constexpr int kPlayBtnX = 797, kPlayBtnY = 466, kPlayBtnW = 260, kPlayBtnH = 52;
void review() {
  box(20, 120, 460, 511);
  heading("SESSION ARCHIVE", 40, 144);
  label("EVENT LOG (JSONL)", 40, 178, kMuted, 2);
  label(recorder_text(g_snapshot->recorder_state), 40, 206, recorder_color(g_snapshot->recorder_state), 3);
  char line[80];
  snprintf(line, sizeof(line), "Saved %lu  Lost %lu  Dropped %lu", static_cast<unsigned long>(g_snapshot->recorder_saved),
           static_cast<unsigned long>(g_snapshot->recorder_missing), static_cast<unsigned long>(g_snapshot->recorder_dropped));
  label(line, 40, 240, g_snapshot->recorder_missing || g_snapshot->recorder_dropped ? kYellow : TFT_WHITE, 2);
  divider(40, 266, 420);
  label("RECEIVER CLIPS (RADIO AUDIO)", 40, 284, kMuted, 2);
  const char* clip_text = !g_snapshot->clips_enabled ? "OFF" : g_snapshot->clip_state == 1 ? "CAPTURING" :
                          g_snapshot->clip_state == 2 ? "WRITING" : "READY";
  label(clip_text, 40, 316, g_snapshot->clips_enabled ? (g_snapshot->clip_state ? kYellow : kGreen) : kMuted, 3);
  snprintf(line, sizeof(line), "Saved %lu  Failed %lu  Skipped %lu", static_cast<unsigned long>(g_snapshot->clips_saved),
           static_cast<unsigned long>(g_snapshot->clips_failed), static_cast<unsigned long>(g_snapshot->clips_refused));
  label(line, 40, 348, g_snapshot->clips_failed ? kYellow : TFT_WHITE, 2);
  button(g_snapshot->clips_enabled ? "CLIPS ON" : "CLIPS OFF", kClipBtnX, kClipBtnY, kClipBtnW,
         g_snapshot->clips_enabled ? kGreen : kCyan, true);
  divider(40, 436, 420);
  label("MICROPHONE CLIPS (ROOM AUDIO)", 40, 454, kMuted, 2);
  const uint8_t m = g_snapshot->mic_state;
  const char* mic_text = m == 0 ? "OFF" : m == 1 ? (g_snapshot->mic_clip_state == 1 ? "CAPTURING" :
                         g_snapshot->mic_clip_state == 2 ? "WRITING" : "LISTENING") :
                         m == 2 ? "PAUSED: SPEAKER ON" : "FAILED";
  label(mic_text, 40, 486, m == 0 ? kMuted : m == 1 ? kRed : m == 2 ? kYellow : kRed, 3);
  snprintf(line, sizeof(line), "Saved %lu  Failed %lu  Skipped %lu", static_cast<unsigned long>(g_snapshot->mic_clips_saved),
           static_cast<unsigned long>(g_snapshot->mic_clips_failed), static_cast<unsigned long>(g_snapshot->mic_clips_refused));
  label(line, 40, 518, g_snapshot->mic_clips_failed ? kYellow : TFT_WHITE, 2);
  button(m == 1 || m == 2 ? "STOP MIC" : "START MIC", kMicBtnX, kMicBtnY, kMicBtnW, m == 1 || m == 2 ? kRed : kCyan, true);
  if (g_snapshot->mic_error[0]) label(g_snapshot->mic_error, 40, 616, kYellow, 2);
  box(497, 120, 763, 511);
  heading("EVIDENCE REVIEW", 517, 144);
  if (!g_snapshot->clip_count) {
    label("NO CLIPS THIS SESSION", 517, 220, kMuted, 3);
    label("A clip is saved when an event fires:", 517, 290, kMuted, 2);
    label("5 s before and 5 s after the event.", 517, 328, kMuted, 2);
    label("Radio clips are the demodulated audio;", 517, 366, kMuted, 2);
    label("mic clips are the room, never mixed.", 517, 404, kMuted, 2);
  } else {
    const uint8_t sel = std::min<uint8_t>(g_snapshot->selected_clip, static_cast<uint8_t>(g_snapshot->clip_count - 1));
    for (uint8_t i = 0; i < g_snapshot->clip_count && i < kClipInfoCount; ++i) {
      const ClipInfo& o = g_snapshot->clips[i];
      const bool chosen = i == sel;
      box(kRowX, kRowY0 + i * kRowDy, kRowW, kRowH, chosen ? (o.mic ? kRed : kGreen) : kEdge);
      snprintf(line, sizeof(line), "%u %s %s  %u.%u s%s", static_cast<unsigned>(i + 1), o.mic ? "MIC  " : "RADIO", o.name,
               static_cast<unsigned>(o.duration_ds / 10), static_cast<unsigned>(o.duration_ds % 10), o.truncated ? "  TRUNCATED" : "");
      label(line, kRowX + 14, kRowY0 + i * kRowDy + kRowH / 2, o.truncated ? kYellow : chosen ? TFT_WHITE : kMuted, 2);
    }
    const ClipInfo& c = g_snapshot->clips[sel];
    if (c.mic)
      snprintf(line, sizeof(line), "ROOM MIC  speaker %s%s", c.speaker_on ? "ON" : "MUTED", c.truncated ? "  TRUNCATED" : "");
    else
      snprintf(line, sizeof(line), "%s  %.3f MHz  speaker %s%s", c.demod, static_cast<double>(c.frequency_hz) / 1e6,
               c.speaker_on ? "ON" : "MUTED", c.truncated ? "  TRUNCATED" : "");
    label(line, 517, 370, c.truncated ? kYellow : TFT_WHITE, 2);
    snprintf(line, sizeof(line), "sha256 %s...", c.sha16);
    label(line, 517, 400, kMuted, 2);
    const uint8_t v = g_snapshot->verify_state;
    label(v == 1 ? "VERIFYING..." : v == 2 ? "VERIFIED: file on the card matches" : v == 3 ? "MISMATCH: file differs from its hash" :
          v == 4 ? "FILE MISSING from the card" : "NOT RE-CHECKED YET", 517, 432,
          v == 2 ? kGreen : (v == 3 || v == 4) ? kRed : v == 1 ? kYellow : kMuted, 2);
    const uint8_t rp = g_snapshot->replay_state;
    button("VERIFY SELECTED", kVerifyBtnX, kVerifyBtnY, kVerifyBtnW, kCyan, v != 1 && rp != 1 && rp != 2);
    button(rp == 2 ? "STOP REPLAY" : rp == 1 ? "LOADING..." : "PLAY SELECTED", kPlayBtnX, kPlayBtnY, kPlayBtnW,
           rp == 2 ? kRed : kGreen, rp != 1);
    label(rp == 2 ? "PLAYING through the speaker; mic paused" : rp == 1 ? "Reading the clip and checking its hash" :
          rp == 3 ? g_snapshot->replay_note : "Plays only if its hash matches the card.", 517, 546,
          rp == 2 ? kGreen : rp == 3 ? kYellow : kMuted, 2);
    label("Replay mutes the radio audio while it plays.", 517, 580, kMuted, 2);
  }
}
void paint_page() {
  switch (g_page) {
    case Page::overview: overview(); break;
    case Page::spectrum: spectrum(); break;
    case Page::audio: audio(); break;
    case Page::environment: environment(); break;
    case Page::events: events(); break;
    case Page::review: review(); break;
    default: break;
  }
}
void paint_direct() {
  g.t = &M5.Display;
  g.oy = 0;
  M5.Display.fillRect(0, kBodyY, 1280, kBodyBottom - kBodyY + 1, kBg);
  paint_page();
  if (g_page == Page::spectrum)  // the waterfall scrolls inside this rectangle, like the LoRa dashboard
    M5.Display.setScrollRect(kWfX + 1, kWfY + 1, kWfW - 2, kWfH - 2, plot_bg());
  if (g_page == Page::audio)  // the envelope rolls inside this one
    M5.Display.setScrollRect(kAuX + 1, kAuY + 1, kAuW - 2, kAuH - 2, plot_bg());
}
// page_change: another screen is replacing this one, so most pixels differ and tile pushes would not help.
void draw_body(bool page_change = false) {
  if (g_info_open) return;  // the popup owns the body until it is closed
  M5Canvas* canvas = body_canvas();
  if (!canvas) { paint_direct(); return; }
  const uint32_t t0 = now_us();
  g.t = canvas;
  g.oy = kBodyY;
  canvas->fillSprite(kBg);
  paint_page();
  g.t = &M5.Display;
  g.oy = 0;
  const uint32_t t1 = now_us();
  acars_ui::DirtyRect rects[kMaxPushRects];
  size_t count = 0;
  bool whole = true;
  if (!page_change && g_front && g_front_valid) {
    const uint16_t* next = static_cast<const uint16_t*>(canvas->getBuffer());
    const uint16_t* previous = static_cast<const uint16_t*>(g_front->getBuffer());
    if (next && previous) {
      bool overflow = false;
      count = acars_ui::diff_rects(previous, next, 1280, kBodyHeight, rects, kMaxPushRects, &overflow);
      const int64_t area = acars_ui::rect_area(rects, count);
      whole = overflow || area * 100 > static_cast<int64_t>(1280) * kBodyHeight * kWholeFramePercent;
      g_stats.pixels = static_cast<uint32_t>(area);
    }
  }
  const uint32_t t2 = now_us();
  if (whole) {
    paint_direct();  // native primitives straight to the panel, like the other dashboards
    ++g_stats.whole_pushes;
    g_stats.pixels = 1280u * kBodyHeight;
  } else {
    for (size_t i = 0; i < count; ++i) {
      M5.Display.setClipRect(rects[i].x, kBodyY + rects[i].y, rects[i].w, rects[i].h);
      canvas->pushSprite(0, kBodyY);
    }
    M5.Display.clearClipRect();
    ++g_stats.partial_pushes;
  }
  const uint32_t t3 = now_us();
  g_stats.compose_us = t1 - t0;
  g_stats.diff_us = t2 - t1;
  g_stats.push_us = t3 - t2;
  g_stats.rects = whole ? 1u : static_cast<uint32_t>(count);
  g_stats.push_us_max = std::max(g_stats.push_us_max, g_stats.push_us);
  if (g_front) {
    std::swap(g_canvas, g_front);  // the frame now on the panel becomes the reference
    g_front_valid = true;
  }
}
} // namespace

bool draw_spectrum_frame(const float* dbfs, size_t count, uint32_t epoch, uint32_t now_ms) {
  if (g_info_open || !g_active || !g_scope || !g_snapshot || !dbfs || count < 2) return false;
  if (!screens::owns(screens::Id::dead_air) || g_page != Page::spectrum) return false;
  const uint32_t t0 = now_us();
  Scope& s = *g_scope;
  const size_t n = std::min(count, kTracePoints);
  const uint16_t bg = plot_bg();
  const int x0 = kPlotX + 1, w = kPlotW - 2, bottom = kPlotY + kPlotH - 2;
  const bool new_epoch = !s.have_epoch || epoch != s.last_epoch;
  if (new_epoch) s.trail.reset();  // a retune or setting change breaks the peak hold
  const float dt = s.last_frame_ms && now_ms > s.last_frame_ms ? static_cast<float>(now_ms - s.last_frame_ms) / 1000.0f : 0.0f;
  s.trail.update(dbfs, n, kTrailDecayDbPerS * dt);
  int16_t xs[kTracePoints], ys[kTracePoints], ts[kTracePoints];
  for (size_t i = 0; i < n; ++i) {
    xs[i] = static_cast<int16_t>(x0 + static_cast<int>(i) * (w - 1) / static_cast<int>(n - 1));
    ys[i] = static_cast<int16_t>(level_y(dbfs[i], kPlotY, kPlotH));
    ts[i] = static_cast<int16_t>(level_y(s.trail.at(i), kPlotY, kPlotH));
  }
  M5.Display.startWrite();
  if (s.prev_n == n) {  // erase last frame's drawing, as the LoRa and Home scopes do
    for (size_t i = 0; i < n; ++i)
      if (bottom > s.prev_y[i] + 2) M5.Display.drawFastVLine(s.prev_x[i], s.prev_y[i] + 2, bottom - s.prev_y[i] - 1, bg);
    for (size_t i = 1; i < n; ++i) {
      for (int dy = -1; dy <= 1; ++dy) M5.Display.drawLine(s.prev_x[i - 1], s.prev_y[i - 1] + dy, s.prev_x[i], s.prev_y[i] + dy, bg);
      M5.Display.drawLine(s.prev_x[i - 1], s.prev_trail_y[i - 1], s.prev_x[i], s.prev_trail_y[i], bg);
    }
  } else {
    M5.Display.fillRect(x0, kPlotY + 1, w, kPlotH - 2, bg);
  }
  draw_plot_grid(M5.Display);
  const uint16_t curtain = viz::rgb565(26, 10, 60);
  for (size_t i = 1; i < n; ++i)  // fading afterimage (peak hold), behind the live trace
    M5.Display.drawLine(xs[i - 1], ts[i - 1], xs[i], ts[i], viz::color_at(s.lut, s.trail.at(i) - 10.0f));
  for (size_t i = 0; i < n; ++i)
    if (bottom > ys[i] + 2) M5.Display.drawFastVLine(xs[i], ys[i] + 2, bottom - ys[i] - 1, curtain);
  for (size_t i = 1; i < n; ++i) {
    const float t = viz::normalize(dbfs[i]);
    const uint16_t halo = s.lut[static_cast<int>(t * 110.0f)];
    const uint16_t core = s.lut[std::min(255, 190 + static_cast<int>(t * 65.0f))];
    M5.Display.drawLine(xs[i - 1], ys[i - 1] - 1, xs[i], ys[i] - 1, halo);
    M5.Display.drawLine(xs[i - 1], ys[i - 1] + 1, xs[i], ys[i] + 1, halo);
    M5.Display.drawLine(xs[i - 1], ys[i - 1], xs[i], ys[i], core);
  }
  for (size_t i = 0; i < n; ++i) { s.prev_x[i] = xs[i]; s.prev_y[i] = ys[i]; s.prev_trail_y[i] = ts[i]; }
  s.prev_n = n;
  // Waterfall: a marker row at every retune/setting change so continuity is never implied, then one measured row.
  if (now_ms - s.last_row_ms >= kWaterfallRowMs || new_epoch) {
    if (s.have_epoch && new_epoch) {
      viz::render_gap_row(s.row, kWfW - 2);
      waterfall_push_row(s);
    }
    viz::render_row(dbfs, n, s.lut, s.row, kWfW - 2);
    waterfall_push_row(s);
    s.last_row_ms = now_ms;
  }
  M5.Display.endWrite();
  s.last_epoch = epoch;
  s.have_epoch = true;
  s.last_frame_ms = now_ms;
  const uint32_t us = now_us() - t0;
  g_stats.frame_us = us;
  g_stats.frame_us_max = std::max(g_stats.frame_us_max, us);
  ++g_stats.frames;
  return true;
}

void audio_text(int x, int y, int w, const char* text, uint16_t color) {
  M5.Display.fillRect(x, y - 11, w, 22, kPanel);
  M5.Display.setTextDatum(middle_left);
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(color);
  M5.Display.drawString(text, x, y);
}

// One envelope bar, mirrored about the centre line, in the column [x, x + kAuStep). Colour rises with the measured level.
void draw_audio_bar(const Scope& s, int x, float norm) {
  const uint16_t bg = plot_bg();
  const int cy = kAuY + kAuH / 2;
  const float t = std::clamp(norm, 0.0f, 1.0f);
  const int h = audio_half(t);
  M5.Display.fillRect(x, kAuY + 1, kAuStep, kAuH - 2, bg);
  for (int db = -20; db >= -60; db -= 20) {
    const int gh = audio_half((static_cast<float>(db) - kAudioFloorDb) / -kAudioFloorDb);
    M5.Display.drawFastHLine(x, cy - gh, kAuStep, plot_grid());
    M5.Display.drawFastHLine(x, cy + gh, kAuStep, plot_grid());
  }
  M5.Display.drawFastHLine(x, cy, kAuStep, plot_grid());
  if (h > 0) {
    M5.Display.fillRect(x, cy - h, kAuStep - 1, 2 * h + 1, s.lut[static_cast<int>(t * 110.0f)]);
    const uint16_t cap = s.lut[std::min(255, 150 + static_cast<int>(t * 105.0f))];
    M5.Display.fillRect(x, cy - h, kAuStep - 1, 2, cap);
    M5.Display.fillRect(x, cy + h - 1, kAuStep - 1, 2, cap);
  }
}

bool draw_audio_frame(const AudioView& v, uint32_t now_ms) {
  if (g_info_open || !g_active || !g_scope || !g_snapshot) return false;
  if (!screens::owns(screens::Id::dead_air) || g_page != Page::audio) return false;
  const uint32_t t0 = now_us();
  Scope& s = *g_scope;
  const uint16_t bg = plot_bg();
  const int cy = kAuY + kAuH / 2;
  const int right = kAuX + kAuW - 1 - kAuStep;  // x of the newest bar
  char text[48];
  M5.Display.startWrite();
  if (!v.flowing || !v.envelope || v.count < 2) {
    if (s.a_state != 0) {  // one plain notice; no trace is invented
      M5.Display.fillRect(kAuX + 1, kAuY + 1, kAuW - 2, kAuH - 2, bg);
      draw_audio_grid(M5.Display, 0);
      M5.Display.setTextDatum(middle_left);
      M5.Display.setTextSize(2);
      M5.Display.setTextColor(kYellow);
      M5.Display.drawString("NO DEMODULATED AUDIO", kAuX + 20, cy - 14);
      M5.Display.setTextColor(kMuted);
      M5.Display.drawString("Receiver idle or between channels.", kAuX + 20, cy + 14);
      audio_text(300, 470, 300, "--", kMuted);
      audio_text(300, 515, 300, "--", kMuted);
      audio_text(300, 560, 300, "--", kMuted);
      audio_text(829, 515, 410, "AUDIO: NONE", kMuted);
      s.a_state = 0;
    }
    M5.Display.endWrite();
    return true;
  }
  if (s.a_state != 1) {
    // First frame after the tab opens or audio returns: start clean and seed the recent history from the envelope.
    M5.Display.fillRect(kAuX + 1, kAuY + 1, kAuW - 2, kAuH - 2, bg);
    draw_audio_grid(M5.Display, 0);
    const size_t n = std::min<size_t>(v.count, static_cast<size_t>((kAuW - 2) / kAuStep));
    for (size_t i = 0; i < n; ++i)
      draw_audio_bar(s, right - static_cast<int>(n - 1 - i) * kAuStep, v.envelope[v.count - n + i]);
    M5.Display.setScrollRect(kAuX + 1, kAuY + 1, kAuW - 2, kAuH - 2, bg);
    s.a_state = 1;
  } else {
    // Roll the plot left with the panel's own scroll and draw only the new bars on the right: nothing is erased and
    // redrawn, so there is no flicker.
    const size_t fresh = v.fresh ? std::min<size_t>(v.fresh_count, 8) : 0;
    for (size_t i = 0; i < fresh; ++i) {
      M5.Display.scroll(-kAuStep, 0);
      draw_audio_bar(s, right, v.fresh[i]);
    }
  }
  // Numbers refresh at about 4 Hz so they stay steady to read.
  if (now_ms - s.a_text_ms >= 250 || s.a_text_ms == 0) {
    s.a_text_ms = now_ms;
    std::snprintf(text, sizeof(text), "%.1f dBFS", static_cast<double>(v.rms_dbfs));
    audio_text(300, 470, 300, text, TFT_WHITE);
    std::snprintf(text, sizeof(text), "%.1f dBFS", static_cast<double>(v.peak_dbfs));
    audio_text(300, 515, 300, text, TFT_WHITE);
    if (v.baseline_ready) std::snprintf(text, sizeof(text), "%.1f dBFS", static_cast<double>(v.baseline_dbfs));
    else std::snprintf(text, sizeof(text), "WARMING UP");
    audio_text(300, 560, 300, text, v.baseline_ready ? TFT_WHITE : kYellow);
    audio_text(829, 515, 410, "AUDIO: FLOWING", kGreen);
  }
  M5.Display.endWrite();
  const uint32_t us = now_us() - t0;
  g_stats.frame_us = us;
  g_stats.frame_us_max = std::max(g_stats.frame_us_max, us);
  ++g_stats.frames;
  return true;
}

void attach_events(EventModel* events) { g_events = events; }
void enter(const Snapshot& snapshot) {
  if (!screens::is_active(screens::Id::dead_air)) return;
  if (!g_snapshot) g_snapshot = new (std::nothrow) Snapshot();
  if (!g_snapshot) return;
  *g_snapshot = snapshot;
  sanitize_snapshot();
  g_page = Page::overview;
  g_info_open = false;
  g_active = true;
  scope_alloc();
  chrome();
  draw_body(true);
}
void update(const Snapshot& snapshot) {
  if (!g_active || !screens::owns(screens::Id::dead_air) ||
      snapshot.revision == g_snapshot->revision) return;
  const int battery = g_snapshot->battery_percent;
  const bool sound = g_snapshot->sound_enabled;
  const uint8_t mic = g_snapshot->mic_state;
  *g_snapshot = snapshot;
  sanitize_snapshot();
  if (g_scope) {  // a lost or settling stream erases the trace; the waterfall history stays
    const bool live = g_snapshot->receiver_ready && g_snapshot->spectrum_valid;
    if (!live && g_scope->was_valid && g_page == Page::spectrum) scope_erase_trace();
    g_scope->was_valid = live;
  }
  if (sound != snapshot.sound_enabled) audio_header::draw_mute_button(snapshot.sound_enabled);
  if (battery != snapshot.battery_percent) audio_header::draw_battery(snapshot.battery_percent);
  if (mic != snapshot.mic_state) mic_banner();
  draw_body();
}
void leave() {
  g_active = false;
  g_info_open = false;
  release_canvas();
  scope_release();
}
bool active() { return g_active; }
PaintStats paint_stats() { return g_stats; }
Page page() { return g_page; }
void close_info() {
  g_info_open = false;
  if (g_scope) { g_scope->a_state = -1; g_scope->a_prev_n = 0; g_scope->prev_n = 0; }  // the plots repaint from scratch
  draw_info_button();
  draw_body(true);
}
Action handle_touch(int32_t x, int32_t y, uint32_t now_ms) {
  if (!g_active || !screens::owns(screens::Id::dead_air)) return {};
  if (audio_header::home_hit(x, y)) return {ActionKind::close};
  if (hit(x, y, kInfoBtnX, kInfoBtnY, kInfoBtnW, kInfoBtnH)) {  // the (i) button works on every tab and toggles the popup
    if (g_info_open) close_info();
    else {
      g_info_open = true;
      g_info_page = 0;
      draw_info_button();
      draw_info_panel();
    }
    return {};
  }
  if (g_info_open) {
    if (hit(x, y, kInfoCloseX, kInfoBtnRowY, kInfoBtnRowW, 52) ||
        !hit(x, y, kInfoPanelX, kInfoPanelY, kInfoPanelW, kInfoPanelH)) {
      close_info();
    } else if (hit(x, y, kInfoPrevX, kInfoBtnRowY, kInfoBtnRowW, 52) && g_info_page > 0) {
      --g_info_page;
      draw_info_panel();
    } else if (hit(x, y, kInfoNextX, kInfoBtnRowY, kInfoBtnRowW, 52) && g_info_page + 1 < info_page_count()) {
      ++g_info_page;
      draw_info_panel();
    }
    return {};
  }
  if (audio_header::mute_hit(x, y)) return {ActionKind::toggle_mute};
  if (audio_header::settings_hit(x, y)) return {ActionKind::open_settings};
  if (y >= kTabY && y < kTabY + kTabHeight) {
    for (uint8_t i = 0; i < static_cast<uint8_t>(Page::count); ++i) {
      if (hit(x, y, kTabX + i * (kTabWidth + kTabGap), kTabY, kTabWidth, kTabHeight)) {
        if (g_page != static_cast<Page>(i)) {
          g_page = static_cast<Page>(i);
          if (g_scope) { g_scope->a_state = -1; g_scope->a_prev_n = 0; g_scope->prev_n = 0; }
          nav(); draw_body(true);
        }
        return {};
      }
    }
  }
  if (g_page == Page::spectrum && g_snapshot->receiver_ready) {
    if (hit(x, y, 366, kSweepY, 230, 52))
      return {g_snapshot->sweeping ? ActionKind::stop_scan : ActionKind::start_scan, 0};
    if (!g_snapshot->sweeping) {
      if (hit(x, y, 606, kSweepY, 300, 52)) return {ActionKind::sweep_preset, 0};
      if (hit(x, y, 916, kSweepY, 320, 52)) return {ActionKind::sweep_dwell, 0};
      if (hit(x, y, 40, kStepY, 128, 52)) return {ActionKind::tune_down, -1};
      if (hit(x, y, 183, kStepY, 128, 52)) return {ActionKind::tune_up, 1};
      for (int i = 0; i < kModeCount; ++i)
        if (hit(x, y, kModeX0 + (i % 3) * kModeDx, kModeY0 + (i / 3) * kModeDy, kModeW, 52))
          return {ActionKind::set_demod, i};
    }
  }
  if (g_page == Page::review && hit(x, y, kClipBtnX, kClipBtnY, kClipBtnW, kClipBtnH)) return {ActionKind::toggle_clips, 0};
  if (g_page == Page::review && hit(x, y, kMicBtnX, kMicBtnY, kMicBtnW, kMicBtnH)) return {ActionKind::toggle_mic, 0};
  if (g_page == Page::review) {
    for (uint8_t i = 0; i < g_snapshot->clip_count && i < kClipInfoCount; ++i)
      if (hit(x, y, kRowX, kRowY0 + i * kRowDy, kRowW, kRowH)) return {ActionKind::select_clip, i};
    const uint8_t rp = g_snapshot->replay_state;
    if (g_snapshot->clip_count && g_snapshot->verify_state != 1 && rp != 1 && rp != 2 &&
        hit(x, y, kVerifyBtnX, kVerifyBtnY, kVerifyBtnW, kVerifyBtnH)) return {ActionKind::verify_clip, 0};
    if (g_snapshot->clip_count && hit(x, y, kPlayBtnX, kPlayBtnY, kPlayBtnW, kPlayBtnH)) {
      if (rp == 2) return {ActionKind::stop_replay, 0};
      if (rp != 1) return {ActionKind::play_clip, 0};
    }
  }
  if (g_page == Page::events && g_events != nullptr) {
    if (hit(x, y, 818, 505, 420, 52)) {
      g_events->mark_manual(now_ms, g_snapshot->unix_time, g_snapshot->frequency_hz);
      draw_body(); return {};
    }
    if (hit(x, y, 818, 382, 421, 52)) {
      g_events->set_sensitivity(x < 956 ? Sensitivity::quiet :
                                 x < 1094 ? Sensitivity::standard : Sensitivity::sensitive);
      draw_body(); return {};
    }
  }
  return {};
}
bool self_check() {
  return static_cast<uint8_t>(Page::count) == 6 && kTabX > 0 &&
    kTabX + 5 * (kTabWidth + kTabGap) + kTabWidth <= 1280 &&
    kBodyBottom < kTabY && kBodyY >= 110;
}

} // namespace orcsdr::dead_air
