// Exercise the real dashboard actions and repaint decisions with a recording display.
#include "ft8_dashboard.hpp"
#include "ft8_map_source.hpp"
#include "ft8_world_projection.hpp"
#include "dashboard_audio_control.hpp"
#include "map_preferences.hpp"
#include "memory_reclaim.hpp"
#include <cstring>
#include <M5Unified.h>
#include <cassert>
#include <cstdio>
namespace orcsdr::audio_header { void draw_brand(const char*) {} }
namespace {
orcsdr::ft8::map_source::State test_frame=orcsdr::ft8::map_source::State::pending;
uint32_t test_map_revision=0;
bool test_labels=true, test_save_fail=false, last_requested_labels=true;
bool test_map_low_memory=false;
int map_requests=0;
int reclaim_calls=0;
bool holder_busy=false;
size_t reclaim_test_buffers() {
  ++reclaim_calls;
  if(holder_busy)return 0;
  lgfx::sprite_fail=false;
  return 15840000;
}
}
namespace orcsdr::maps::preferences {
void load() {}
bool labels(Consumer c) { return c==Consumer::ft8 ? test_labels : true; }
bool set_labels(Consumer c,bool on) {
  if(c!=Consumer::ft8 || test_save_fail)return false;
  test_labels=on;return true;
}
}
namespace orcsdr::ft8::map_source {
Result draw(lgfx::LovyanGFX&,bool labels) {
  ++map_requests;last_requested_labels=labels;
  Result r{};r.state=test_frame;
  std::strcpy(r.credit,"Made with Natural Earth");
  r.low_memory=test_map_low_memory;
  return r;
}
uint32_t revision(){return test_map_revision;}
}
using namespace orcsdr::ft8;
bool painted(const char* s) {
  for (const auto& p : lgfx::paints) if (p.text.find(s) != std::string::npos) return true;
  return false;
}
int painted_x(const char* s) {
  for (const auto& p : lgfx::paints) if (p.text==s) return p.x;
  return -1;
}
int main() {
  Snapshot s;
  s.mode = DigitalMode::ft4;
  s.decoder_state = DecoderState::armed;
  s.dial_hz = 14080000;
  s.expert_tuning = true;
  s.gain_available = true;
  s.gain_step_count = 2;
  s.gain_steps_tenth_db[1] = 100;
  enter(s);
  assert(painted("7.5 second"));
  select_tab(Tab::hunter);
  const Action band_pick = handle_touch(360, 375); // 20 m card, row 1 / column 1
  assert(band_pick.kind == ActionKind::tune_band && band_pick.value == 14080000);
  assert(painted_x("FT4 RX")>=420); // shared map notice ends at x412
  select_tab(Tab::live);
  (void)handle_touch(80, 130); // open Tune
  const Action first = handle_touch(885, 310); // + step
  const Action second = handle_touch(885, 310);
  assert(first.kind == ActionKind::tune_dial && second.value == first.value);
  select_tab(Tab::heard);
  lgfx::paints.clear();
  s.station_known = true;
  s.station_latitude = 44.0f;
  s.station_longitude = -123.0f;
  update(s);
  assert(!lgfx::paints.empty());
  select_tab(Tab::live);
  (void)handle_touch(1100, 130); // gain popup
  lgfx::paints.clear();
  s.gain_auto = false;
  s.gain_tenth_db = 100;
  update(s);
  assert(painted("AUTO") && painted("MANUAL"));
  // The RTL2832 AGC (V3/V4 dongles) has its own button in the gain popup: hidden when the dongle lacks it, a toggle when present.
  assert(!painted("RTL AGC"));
  assert(handle_touch(750, 178).kind != ActionKind::rtl_agc);
  s.rtl_agc_available = true;
  lgfx::paints.clear();
  update(s);
  assert(painted("RTL AGC OFF"));
  const Action agc_on = handle_touch(750, 178);
  assert(agc_on.kind == ActionKind::rtl_agc && agc_on.value == 1);
  s.rtl_agc = true;
  lgfx::paints.clear();
  update(s);
  assert(painted("RTL AGC ON"));
  const Action agc_off = handle_touch(750, 178);
  assert(agc_off.kind == ActionKind::rtl_agc && agc_off.value == 0);
  // The button does not depend on the tuner gain being available (dongles whose tuner gain is unusable still have the RTL2832 AGC).
  s.gain_available = false;
  s.rtl_agc = false;   // changes with gain_available so the popup repaints
  lgfx::paints.clear();
  update(s);
  assert(painted("RTL AGC OFF"));
  const Action agc_no_gain = handle_touch(750, 178);
  assert(agc_no_gain.kind == ActionKind::rtl_agc && agc_no_gain.value == 1);
  s.gain_available = true;
  // The decoder could not start for lack of memory: the screen says so (not "not bound"), shows the numbers and who holds memory,
  // and the FREE MEMORY button works only when something can actually be released.
  leave();   // closes the gain popup left open above
  lgfx::paints.clear();
  s.decoder_state = DecoderState::no_memory;
  s.mem_free = 262144;
  s.mem_largest = 163840;
  s.mem_needed = 4194304;
  static char holders[] = "SSTV pictures 3.8 MB";
  s.mem_holders = holders;
  s.mem_holders_rev = 1;
  enter(s);
  select_tab(Tab::live);
  assert(painted("NOT ENOUGH MEMORY") && !painted("DECODER NOT BOUND"));
  assert(painted("0.2 MB") && painted("4.0 MB") && painted("SSTV pictures 3.8 MB") && painted("FREE MEMORY"));
  const Action free_it = handle_touch(466, 440);   // the FREE MEMORY button
  assert(free_it.kind == ActionKind::free_memory);
  s.mem_holders = nullptr;   // nothing left that a feature can release
  s.mem_holders_rev = 2;
  update(s);
  assert(painted("restart OrcSDR"));
  assert(handle_touch(466, 440).kind != ActionKind::free_memory);   // nothing to release: the button is disabled
  s.decoder_state = DecoderState::unbound;
  lgfx::paints.clear();
  update(s);
  assert(painted("DECODER NOT BOUND") && !painted("NOT ENOUGH MEMORY"));
  // The CLOCK chip reports whether the clock is known to be right (a recent network sync), not just that a date exists; tapping it asks for a sync.
  s.clock_valid = true;
  s.clock_sync_level = 0;   // unsynced
  lgfx::paints.clear();
  update(s);
  assert(painted("UNSYNCED") && painted("CLOCK - TAP"));
  assert(handle_touch(700, 130).kind == ActionKind::open_clock);
  assert(handle_touch(700, 75).kind == ActionKind::open_clock);
  s.clock_sync_level = 2;   // synced
  s.clock_sync_age_s = 12 * 60;
  lgfx::paints.clear();
  update(s);
  assert(painted("SYNCED 12m"));
  s.clock_sync_age_s = 13 * 60;   // the age text moves on with the minute
  lgfx::paints.clear();
  update(s);
  assert(painted("SYNCED 13m"));
  lgfx::paints.clear();
  select_tab(Tab::decodes);
  assert(painted("SYNCED 13m"));
  assert(handle_touch(900, 130).kind == ActionKind::open_clock);
  lgfx::paints.clear();
  select_tab(Tab::setup);
  assert(painted("SYNCED 13 MIN AGO"));
  // The new FT8 map uses the pinned world worker, not the retired outline;
  // pending frames never undergo pixel reprojection.
  lgfx::paints.clear();lgfx::sampled_pixels=0;lgfx::drawn_pixels=0;
  test_frame=map_source::State::pending;
  select_tab(Tab::map);
  assert(painted("MAIDENHEAD GRID") && painted("LABELS ON") && painted("LOADING WORLD"));
  assert(last_requested_labels && lgfx::sampled_pixels==0);
  ++test_map_revision;test_frame=map_source::State::ready;
  lgfx::paints.clear();update(s);
  assert(painted("ORCMAPS WORLD") && lgfx::sampled_pixels>0 && lgfx::drawn_pixels>0);
  assert(lgfx::row_reads>0&&!lgfx::row_images.empty());
  for(const auto& row:lgfx::row_images) {
    assert(row.x==0&&row.pixels.size()==824);
    for(int col=0;col<824;++col) {
      int sx=0,sy=0;assert(world_map::source_pixel(col,row.y,824,420,&sx,&sy));
      assert(row.pixels[col]==lgfx::pixel_color(sx,sy));
    }
  }

  // Worker publishes revision while its mutex is still held: pending may be
  // observed once even though that revision's completed frame is now available.
  ++test_map_revision;test_frame=map_source::State::pending;
  lgfx::paints.clear();update(s);
  assert(lgfx::paints.empty()); // a locked pending publication never repaints static panels
  for(int repeat=0;repeat<5;++repeat){update(s);assert(lgfx::paints.empty());}
  test_millis+=250;
  test_frame=map_source::State::ready;
  lgfx::paints.clear();update(s);
  assert(painted("ORCMAPS WORLD"));
  const int old_you_x=painted_x("YOU");
  assert(old_you_x>0);
  test_frame=map_source::State::pending;
  s.station_longitude=-45.0f;
  lgfx::paints.clear();update(s);
  assert(painted("LAST MAP FRAME") && painted_x("YOU")==old_you_x);
  // Only when the new complete world frame is ready may the receiver
  // cross AND its separately painted caption move to the new position.
  ++test_map_revision;test_frame=map_source::State::ready;
  lgfx::paints.clear();update(s);
  assert(painted("ORCMAPS WORLD") && painted_x("YOU")>old_you_x);
  test_save_fail=true;
  (void)handle_touch(770,130);
  assert(test_labels && last_requested_labels); // failed persistence never flips UI or renderer
  test_save_fail=false;
  (void)handle_touch(770,130);
  assert(!test_labels && !last_requested_labels && painted("LABELS OFF"));
  assert(lgfx::sampled_pixels>0); // completed off-label generation uses same geometry
  test_frame=map_source::State::pending;
  lgfx::paints.clear();(void)handle_touch(770,130);
  assert(test_labels && painted("APPLYING LABELS")); // old labelled bitmap is declared pending
  test_frame=map_source::State::unavailable;
  ++test_map_revision;
  lgfx::paints.clear();update(s);
  assert(painted("LAST MAP FRAME")); // no repeated blank after an unavailable worker
  leave();
  // Fresh entry with simulated PSRAM allocation failure: a truthful grid only.
  lgfx::sprite_fail=true;
  enter(s);
  lgfx::paints.clear();select_tab(Tab::map);
  assert(painted("LOW MEMORY - GRID ONLY") && painted("LABELS ON"));
  // A busy holder must not strand the map until another navigation or reboot.
  orcsdr::memory_reclaim::register_holder({"test inactive buffers", []()->size_t{return 15840000;},reclaim_test_buffers});
  holder_busy=true;
  test_millis+=2000;
  const int clears=lgfx::screen_clears;
  lgfx::paints.clear();update(s);
  assert(reclaim_calls==1 && painted("LOW MEMORY - GRID ONLY"));
  for(int i=0;i<10;++i)update(s);
  assert(reclaim_calls==1); // no reclaim loop on every receiver update
  holder_busy=false;test_frame=map_source::State::ready;
  test_millis+=2000;
  lgfx::paints.clear();update(s);
  assert(reclaim_calls==2 && painted("ORCMAPS WORLD"));
  assert(lgfx::screen_clears==clears);
  // Once healthy, ordinary updates must not evict the completed map cache.
  test_millis+=2000;update(s);assert(reclaim_calls==2);
  // Worker allocation failure also retries without a new worker revision.
  test_map_low_memory=true;test_frame=map_source::State::unavailable;
  ++test_map_revision;lgfx::paints.clear();update(s);
  assert(painted("LAST MAP FRAME"));
  test_map_low_memory=false;test_frame=map_source::State::ready;
  test_millis+=2000;lgfx::paints.clear();update(s);
  assert(reclaim_calls==3 && painted("ORCMAPS WORLD"));
  assert(lgfx::screen_clears==clears);
  lgfx::sprite_fail=false;
  leave();
  // A fresh clock is not a prerequisite for geography or map controls.
  s.clock_valid=false;s.utc_ms=0;s.decoder_state=DecoderState::armed;
  test_frame=map_source::State::ready;
  enter(s);lgfx::paints.clear();lgfx::sampled_pixels=0;
  select_tab(Tab::map);
  assert(painted("ORCMAPS WORLD") && painted("LABELS ON") && lgfx::sampled_pixels>0);
  const int no_clock_clears=lgfx::screen_clears;
  ++test_map_revision;lgfx::paints.clear();update(s);
  assert(painted("ORCMAPS WORLD") && lgfx::screen_clears==no_clock_clears);
  leave();
  // World-sprite failure must not retain the other half of an unusable pair.
  enter(s);const auto before_partial=lgfx::live_sprite_bytes;
  lgfx::sprite_fail_width=512;lgfx::paints.clear();select_tab(Tab::map);
  assert(painted("LOW MEMORY - GRID ONLY"));
  assert(lgfx::live_sprite_bytes==before_partial);
  lgfx::sprite_fail_width=0;test_millis+=2000;
  lgfx::paints.clear();update(s);assert(painted("ORCMAPS WORLD"));
  leave();
  std::puts("ft8_dashboard_tests: PASS");
}
