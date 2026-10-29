#pragma once
#include "dead_air_model.hpp"
#include <cstdint>

namespace orcsdr::dead_air {

enum class ActionKind : uint8_t {
  none, close, toggle_mute, open_settings, start_scan, stop_scan,
  start_recording, stop_recording, save_clip, export_session, play_clip,
  tune_up, tune_down, set_demod, sweep_preset, sweep_dwell, toggle_clips, verify_clip, toggle_mic, select_clip, stop_replay
};
// Real receiver demodulators only. Sweep patterns and generated audio are not demodulators.
constexpr const char* kModeNames[] = {"NFM", "WFM", "AM", "USB", "LSB"};
constexpr int kModeCount = 5;

struct Action { ActionKind kind = ActionKind::none; int32_t value = 0; };

// All of these entry points run on the OrcSDR UI task. They do not own
// receiver, USB, DSP, I2C or SD hardware. Live observations are supplied
// through Snapshot by an application-level adapter.
void attach_events(EventModel* model);
void enter(const Snapshot& snapshot);
void update(const Snapshot& snapshot);
void leave();
bool active();
struct PaintStats {
  uint32_t compose_us = 0, diff_us = 0, push_us = 0, push_us_max = 0;  // last body refresh
  uint32_t pixels = 0, rects = 0, whole_pushes = 0, partial_pushes = 0;
  uint32_t frame_us = 0, frame_us_max = 0, frames = 0;  // direct-drawn spectrum frames
};
PaintStats paint_stats();

// Live spectrum, drawn straight onto the panel (UI task only). `dbfs` holds `count` measured points in relative dBFS.
// `epoch` changes whenever a retune or setting change makes earlier rows incomparable; the waterfall then gets a marker
// row. Returns false (drawing nothing) unless the Spectrum tab is the visible, owned screen.
constexpr size_t kTracePoints = 216;
bool draw_spectrum_frame(const float* dbfs, size_t count, uint32_t epoch, uint32_t now_ms);

// Live audio level view for the Audio tab, drawn straight onto the panel like the spectrum. `envelope` holds `count`
// peak levels normalised 0..1 over -70..0 dBFS (oldest first). All levels are measured, never synthesised; with
// `flowing` false nothing is drawn but a plain "not demodulating" notice. Returns false off the Audio tab.
struct AudioView {
  const float* envelope = nullptr;  // the whole recent history, used to seed the view
  size_t count = 0;
  const float* fresh = nullptr;     // bars measured since the last call (oldest first); the view scrolls by these
  size_t fresh_count = 0;
  bool flowing = false;
  float rms_dbfs = -100.0f, peak_dbfs = -100.0f;
  bool baseline_ready = false;
  float baseline_dbfs = 0.0f;
};
bool draw_audio_frame(const AudioView& view, uint32_t now_ms);
Page page();
Action handle_touch(int32_t x, int32_t y, uint32_t now_ms);
bool self_check();

} // namespace orcsdr::dead_air
