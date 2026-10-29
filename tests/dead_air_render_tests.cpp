// Compiles the actual Dead Air M5GFX renderer with the existing ACARS
// recording stand-in; no hardware or simulated live RF data.
#include <M5Unified.h>
#include "dead_air_dashboard.hpp"
#include "screen_controller.hpp"
#include <cassert>
#include <string>
#include <cstdio>
using namespace orcsdr;
namespace da = orcsdr::dead_air;
namespace screen = orcsdr::screens;

static bool displayed(const char* text) {
  for (const Prim& p : host_display::prims)
    if (p.kind == Prim::text && p.str.find(text) != std::string::npos) return true;
  return false;
}
static void validate_bounds() {
  for (const Prim& p : host_display::prims) {
    int x, y, w, h;
    if (!host_display::effective(p, &x, &y, &w, &h)) continue;
    assert(x >= 0 && y >= 0 && x + w <= 1280 && y + h <= 720);
  }
}
int main() {
  da::EventModel model;
  model.reset();
  da::attach_events(&model);
  da::Snapshot snap{};
  snap.revision = 1;
  snap.battery_percent = -1;
  // Never admit cached values from an absent ENV III or detached receiver.
  snap.temperature_c = {99.0f, 0, 0, true, true, true};
  snap.environment_sensor_attached = false;
  snap.rf_level_dbfs = {-14.0f, 0, 0, true, true, true};
  snap.receiver_ready = false;
  screen::begin_transition(screen::Id::dead_air, 100);
  da::enter(snap);
  screen::finish_transition();
  assert(da::active() && da::page() == da::Page::overview);
  assert(displayed("MODULE NOT CONNECTED"));
  assert(!displayed("99.0") && !displayed("-14.0"));
  assert(displayed("RECEIVER UNAVAILABLE"));
  validate_bounds();
  for (int i = 0; i < 6; ++i) {
    host_display::reset();
    const int x = 20 + i * 210 + 40;
    auto action = da::handle_touch(x, 673, 1000 + i);
    assert(action.kind == da::ActionKind::none);
    assert(da::page() == static_cast<da::Page>(i));
    // An already active tab must not cause a redundant screen repaint.
    if (i == 0) assert(host_display::prims.empty());
    else assert(!host_display::prims.empty());
    validate_bounds();
  }
  // Sensitivity buttons/marker are real touch actions only on Events.
  da::handle_touch(20 + 4*210 + 40, 673, 2000);
  assert(da::page() == da::Page::events);
  da::handle_touch(1140, 400, 2100);
  assert(model.sensitivity() == da::Sensitivity::sensitive);
  da::handle_touch(900, 530, 2200);
  assert(model.count() == 1 && model.newest(0)->source == da::Source::manual);
  assert(displayed("MARK no receiver"));
  // Mode and step buttons act only while a receiver is actually present, and only on the Spectrum tab.
  da::handle_touch(20 + 1 * 210 + 40, 673, 2300);
  assert(da::page() == da::Page::spectrum);
  assert(da::handle_touch(145, 370, 2310).kind == da::ActionKind::none);
  snap.receiver_ready = true;
  snap.receiver_settled = true;
  snap.frequency_hz = 98700000;
  std::snprintf(snap.demod, sizeof(snap.demod), "WFM");
  snap.revision = 5;
  da::update(snap);
  auto mode = da::handle_touch(145, 370, 2320);
  assert(mode.kind == da::ActionKind::set_demod && mode.value == 4 && std::string(da::kModeNames[4]) == "LSB");
  mode = da::handle_touch(40 + 95 + 10, 290 + 20, 2330);
  assert(mode.kind == da::ActionKind::set_demod && mode.value == 1);
  assert(da::handle_touch(200, 470, 2340).kind == da::ActionKind::tune_up);
  assert(da::handle_touch(60, 470, 2350).kind == da::ActionKind::tune_down);
  // A measured spectrum draws the ectoplasm trace, waterfall and legend inside the Spectrum panel; a retune, a new
  // frame and a loss of the receiver all stay in bounds and never draw a trace without valid data.
  for (size_t i = 0; i < da::kBins; ++i) snap.spectrum_dbfs[i] = -100.0f + static_cast<float>(i % 40);
  snap.spectrum_valid = true;
  snap.spectrum_seq = 1;
  snap.revision = 20;
  host_display::reset();
  da::update(snap);
  assert(displayed("-110") && displayed("-10 dBFS") && !displayed("NO LIVE SPECTRUM"));
  validate_bounds();
  snap.spectrum_seq = 2;
  snap.frequency_hz = 98900000;
  snap.revision = 21;
  da::update(snap);
  validate_bounds();
  snap.spectrum_valid = false;
  snap.revision = 22;
  host_display::reset();
  da::update(snap);
  assert(displayed("SETTLING"));
  validate_bounds();
  snap.spectrum_valid = true;
  snap.revision = 23;
  da::update(snap);
  // Live frames are drawn straight onto the panel on the Spectrum tab only, and stay inside the screen.
  float pts[da::kTracePoints];
  for (size_t i = 0; i < da::kTracePoints; ++i) pts[i] = -95.0f + static_cast<float>(i % 60);
  host_display::reset();
  assert(da::draw_spectrum_frame(pts, da::kTracePoints, 1, 5000));
  assert(!host_display::prims.empty());
  validate_bounds();
  assert(da::draw_spectrum_frame(pts, da::kTracePoints, 1, 5033));
  assert(da::draw_spectrum_frame(pts, da::kTracePoints, 2, 5200));  // retune: marker row, still bounded
  validate_bounds();
  assert(!da::draw_spectrum_frame(nullptr, 0, 1, 5300));
  assert(da::paint_stats().frames == 3);
  // Audio tab: static frame plus a directly drawn envelope; nothing is drawn off the tab, and "no audio" is explicit.
  assert(!da::draw_audio_frame({}, 7000));  // Spectrum tab is showing
  da::handle_touch(20 + 2 * 210 + 40, 673, 6990);
  assert(da::page() == da::Page::audio);
  assert(displayed("PEAK ENVELOPE") && displayed("RMS LEVEL") && displayed("Level meter only"));
  host_display::reset();
  assert(da::draw_audio_frame({}, 7000) && displayed("NO DEMODULATED AUDIO"));
  validate_bounds();
  float env[da::kBins];
  for (size_t i = 0; i < da::kBins; ++i) env[i] = static_cast<float>(i % 10) / 10.0f;
  da::AudioView view{};
  view.envelope = env; view.count = da::kBins; view.flowing = true;
  view.rms_dbfs = -31.2f; view.peak_dbfs = -12.5f; view.baseline_ready = false;
  host_display::reset();
  assert(da::draw_audio_frame(view, 7100));
  assert(displayed("-31.2 dBFS") && displayed("-12.5 dBFS") && displayed("WARMING UP") && displayed("AUDIO: FLOWING"));
  assert(!displayed("NO DEMODULATED AUDIO"));
  validate_bounds();
  view.baseline_ready = true; view.baseline_dbfs = -33.0f;
  float fresh[2] = {0.4f, 0.9f};
  view.fresh = fresh; view.fresh_count = 2;
  host_display::reset();
  assert(da::draw_audio_frame(view, 7500) && displayed("-33.0 dBFS"));  // numbers refresh at 4 Hz
  validate_bounds();
  assert(da::draw_audio_frame(view, 7550));  // between text refreshes: only the scroll and new bars
  validate_bounds();
  da::handle_touch(20 + 1 * 210 + 40, 673, 7300);  // back to Spectrum for the sweep checks below
  assert(da::page() == da::Page::spectrum);
  // Sweep controls: start/stop, preset and dwell are real actions; tuning and mode buttons yield while it runs.
  assert(da::handle_touch(366 + 20, 530 + 20, 2360).kind == da::ActionKind::start_scan);
  assert(da::handle_touch(606 + 20, 530 + 20, 2361).kind == da::ActionKind::sweep_preset);
  assert(da::handle_touch(916 + 20, 530 + 20, 2362).kind == da::ActionKind::sweep_dwell);
  snap.sweeping = true;
  snap.sweep_hops = 3;
  snap.sweep_dwell_ms = 500;
  snap.revision = 6;
  da::update(snap);
  assert(displayed("STOP SWEEP") && displayed("RF DETECTOR OFF") && displayed("HOP 3"));
  assert(da::handle_touch(366 + 20, 530 + 20, 2370).kind == da::ActionKind::stop_scan);
  assert(da::handle_touch(200, 470, 2371).kind == da::ActionKind::none);
  assert(da::handle_touch(145, 370, 2372).kind == da::ActionKind::none);
  assert(da::handle_touch(606 + 20, 530 + 20, 2373).kind == da::ActionKind::none);
  snap.sweeping = false;
  snap.revision = 7;
  da::update(snap);
  validate_bounds();
  // Events tab: rows carry time and level, and an event that has a clip says so.
  da::handle_touch(20 + 4 * 210 + 40, 673, 8000);
  assert(da::page() == da::Page::events);
  snap.clip_event_seqs[0] = model.newest(0)->sequence;
  snap.clip_event_seq_count = 1;
  snap.revision = 30;
  host_display::reset();
  da::update(snap);
  assert(displayed("MARK no receiver") && displayed("CLIP") && displayed("RF RISE + AUDIO RISE"));
  validate_bounds();
  // Review tab: newest clip details, a VERIFY action, and the re-check result.
  da::handle_touch(20 + 5 * 210 + 40, 673, 8100);
  assert(da::page() == da::Page::review);
  snap.clip_count = 2;
  std::snprintf(snap.clips[0].name, sizeof(snap.clips[0].name), "s1792536903_e1.wav");
  std::snprintf(snap.clips[0].sha16, sizeof(snap.clips[0].sha16), "9f932fde12345678");
  std::snprintf(snap.clips[0].demod, sizeof(snap.clips[0].demod), "NFM");
  snap.clips[0].duration_ds = 100; snap.clips[0].frequency_hz = 97289000; snap.clips[0].speaker_on = false;
  std::snprintf(snap.clips[1].name, sizeof(snap.clips[1].name), "s1792536903_e0.wav");
  snap.clips[1].duration_ds = 87; snap.clips[1].truncated = true;
  snap.verify_state = 0;
  snap.revision = 31;
  host_display::reset();
  da::update(snap);
  assert(displayed("s1792536903_e1.wav") && displayed("NFM  97.289 MHz  speaker MUTED") && displayed("sha256 9f932fde12345678"));
  assert(displayed("VERIFY SELECTED") && displayed("PLAY SELECTED") && displayed("NOT RE-CHECKED YET") && displayed("8.7 s  TRUNCATED"));
  assert(displayed("1 RADIO s1792536903_e1.wav") && displayed("2 RADIO s1792536903_e0.wav"));
  validate_bounds();
  // Selecting the second row moves the details to that clip.
  {
    const da::Action pick = da::handle_touch(600, 172 + 44 + 20, 8150);
    assert(pick.kind == da::ActionKind::select_clip && pick.value == 1);
    snap.selected_clip = 1; snap.revision = 310;
    host_display::reset();
    da::update(snap);
    assert(displayed("sha256 ") && displayed("speaker MUTED  TRUNCATED"));
    snap.selected_clip = 0; snap.revision = 311;
    host_display::reset();
    da::update(snap);
  }
  assert(da::handle_touch(537, 490, 8200).kind == da::ActionKind::verify_clip);
  assert(da::handle_touch(817, 490, 8210).kind == da::ActionKind::play_clip);
  snap.replay_state = 1; snap.revision = 312;
  host_display::reset();
  da::update(snap);
  assert(displayed("LOADING...") && displayed("Reading the clip and checking its hash"));
  assert(da::handle_touch(817, 490, 8220).kind == da::ActionKind::none && da::handle_touch(537, 490, 8221).kind == da::ActionKind::none);
  snap.replay_state = 2; snap.revision = 313;
  host_display::reset();
  da::update(snap);
  assert(displayed("STOP REPLAY") && displayed("PLAYING through the speaker; mic paused"));
  assert(da::handle_touch(817, 490, 8230).kind == da::ActionKind::stop_replay && da::handle_touch(537, 490, 8231).kind == da::ActionKind::none);
  snap.replay_state = 3; snap.revision = 314;
  std::snprintf(snap.replay_note, sizeof(snap.replay_note), "Hash mismatch: not played");
  host_display::reset();
  da::update(snap);
  assert(displayed("Hash mismatch: not played"));
  snap.replay_state = 0; snap.revision = 315;
  host_display::reset();
  da::update(snap);
  snap.verify_state = 1; snap.revision = 32;
  host_display::reset();
  da::update(snap);
  assert(displayed("VERIFYING..."));
  assert(da::handle_touch(537, 490, 8300).kind == da::ActionKind::none);   // not while a check is running
  snap.verify_state = 2; snap.revision = 33;
  host_display::reset();
  da::update(snap);
  assert(displayed("VERIFIED: file on the card matches"));
  snap.verify_state = 3; snap.revision = 34;
  host_display::reset();
  da::update(snap);
  assert(displayed("MISMATCH"));
  validate_bounds();
  // Microphone: off by default, explicit start, always-visible banner, paused state is stated, mic clips are labelled.
  assert(displayed("MICROPHONE CLIPS (ROOM AUDIO)") && displayed("START MIC") && !displayed("MIC LISTENING"));
  assert(da::handle_touch(60, 570, 8450).kind == da::ActionKind::toggle_mic);
  assert(da::handle_touch(60, 390, 8460).kind == da::ActionKind::toggle_clips);
  snap.mic_state = 1; snap.revision = 35;
  host_display::reset();
  da::update(snap);
  assert(displayed("MIC LISTENING") && displayed("STOP MIC") && displayed("LISTENING"));
  validate_bounds();
  snap.mic_state = 2; snap.revision = 36;
  host_display::reset();
  da::update(snap);
  assert(displayed("MIC PAUSED: SPEAKER ON") && displayed("PAUSED: SPEAKER ON"));
  snap.mic_state = 3; snap.revision = 37;
  std::snprintf(snap.mic_error, sizeof(snap.mic_error), "mic_begin_failed");
  host_display::reset();
  da::update(snap);
  assert(displayed("MIC FAILED") && displayed("mic_begin_failed"));
  snap.mic_state = 1; snap.mic_error[0] = 0;
  snap.clips[0].mic = true; snap.clips[0].speaker_on = false;
  snap.revision = 38;
  host_display::reset();
  da::update(snap);
  assert(displayed("ROOM MIC  speaker MUTED") && displayed("1 MIC   s1792536903_e1.wav") && !displayed("97.289 MHz"));
  validate_bounds();
  snap.mic_state = 0; snap.clips[0].mic = false; snap.revision = 39;
  host_display::reset();
  da::update(snap);
  assert(!displayed("MIC LISTENING"));
  // (i) popup: opens on every tab with that tab's text, pages, hides the body, and closes cleanly.
  host_display::reset();
  assert(da::handle_touch(396, 39, 8470).kind == da::ActionKind::none);
  assert(displayed("ABOUT THIS PAGE  -  REVIEW") && displayed("SESSION ARCHIVE") && displayed("PAGE 1/2"));
  validate_bounds();
  host_display::reset();
  da::handle_touch(425, 586, 8480);  // NEXT is at x 350..500
  assert(displayed("PAGE 2/2") && displayed("Each replay is noted in the log"));
  host_display::reset();
  snap.revision = 400;
  da::update(snap);  // a new snapshot while the popup is open must not paint over it
  assert(!displayed("EVIDENCE REVIEW"));
  assert(da::handle_touch(1000, 586, 8490).kind == da::ActionKind::none);  // CLOSE
  assert(displayed("EVIDENCE REVIEW"));
  da::handle_touch(396, 39, 8500);
  da::handle_touch(50, 300, 8510);  // a tap outside the popup closes it
  assert(displayed("EVIDENCE REVIEW"));
  da::handle_touch(20 + 0 * 210 + 40, 673, 8520);  // Overview has its own text
  da::handle_touch(396, 39, 8530);
  assert(displayed("ABOUT THIS PAGE  -  OVERVIEW") && displayed("does not detect ghosts"));
  da::handle_touch(1000, 586, 8540);
  da::handle_touch(20 + 5 * 210 + 40, 673, 8550);
  da::handle_touch(20 + 1 * 210 + 40, 673, 8400);  // back to Spectrum
  // Once another screen takes the framebuffer, Dead Air must be silent.
  screen::begin_transition(screen::Id::home, 3000);
  screen::finish_transition();
  host_display::reset();
  snap.revision = 2;
  da::update(snap);
  assert(host_display::prims.empty());
  assert(da::handle_touch(900, 530, 3200).kind == da::ActionKind::none);
  da::leave();
  assert(!da::active());
  std::puts("Dead Air renderer host tests PASS");
}
