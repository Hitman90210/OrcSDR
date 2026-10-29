#include "dead_air_sweep.hpp"
#include "dead_air_journal.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>

using namespace orcsdr::dead_air;

int main() {
  // Invalid configurations never start.
  Sweep s;
  assert(!s.start({0, 0, 0, 0}, 1, 0) && !s.running());
  assert(!s.start({88000000, 88000000, 200000, 500}, 1, 0));
  assert(!s.start({88000000, 90000000, 0, 500}, 1, 0));
  assert(!s.start({88000000, 90000000, 200000, 0}, 1, 0));

  // Forward: commands start first, waits for settle, dwells, wraps at the end.
  SweepConfig c{100000000, 100400000, 200000, 500, SweepPattern::forward};
  assert(s.start(c, 98700000, 1000));
  uint32_t hz = 0;
  assert(s.tick(1000, true, false, &hz) && hz == 100000000 && s.hops() == 1);
  assert(!s.tick(1100, true, false, &hz));             // not settled: no new command
  assert(!s.tick(1250, true, true, &hz));              // settles; 250 ms of this hop were unobserved
  assert(s.unobserved_ms() == 250);
  assert(!s.tick(1700, true, true, &hz));              // 450 ms of dwell < 500
  assert(s.tick(1750, true, true, &hz) && hz == 100200000 && s.hops() == 2);
  assert(!s.tick(2000, true, true, &hz));             // hop 2 settles: another 250 ms unobserved
  assert(s.unobserved_ms() == 500);
  assert(s.tick(2500, true, true, &hz) && hz == 100400000 && s.hops() == 3);

  // Wrap and bounce use only channels inside the range.
  Sweep f; f.start({1000, 1300, 100, 1, SweepPattern::forward}, 0, 0);
  uint32_t t = 0, seen[8]{}; int n = 0;
  while (n < 6) {
    uint32_t h = 0;
    if (f.tick(t, true, true, &h)) seen[n++] = h;
    t += 5;
  }
  assert(seen[0] == 1000 && seen[1] == 1100 && seen[2] == 1200 && seen[3] == 1300 && seen[4] == 1000);
  Sweep b; b.start({1000, 1200, 100, 1, SweepPattern::bounce}, 0, 0);
  t = 0; n = 0;
  while (n < 6) {
    uint32_t h = 0;
    if (b.tick(t, true, true, &h)) seen[n++] = h;
    t += 5;
  }
  assert(seen[0] == 1000 && seen[1] == 1100 && seen[2] == 1200 && seen[3] == 1100 && seen[4] == 1000 && seen[5] == 1100);
  for (int i = 0; i < 6; ++i) assert(seen[i] >= 1000 && seen[i] <= 1200);

  // A channel that never settles is abandoned and counted; its wait is unobserved time.
  Sweep st; st.start({1000, 2000, 100, 100, SweepPattern::forward}, 0, 0);
  uint32_t h = 0;
  assert(st.tick(0, true, false, &h) && h == 1000);
  assert(!st.tick(2999, true, false, &h));
  assert(st.tick(3000, true, false, &h) && h == 1100 && st.stalls() == 1 && st.unobserved_ms() == 3000);

  // No receiver: the schedule pauses and counts nothing.
  Sweep p; p.start({1000, 2000, 100, 100, SweepPattern::forward}, 0, 0);
  assert(p.tick(0, true, false, &h));
  const uint32_t hops_before = p.hops();
  assert(!p.tick(500, false, false, &h) && !p.tick(5000, false, false, &h));
  assert(p.hops() == hops_before && p.unobserved_ms() == 0 && p.stalls() == 0);

  // Stop leaves the resume frequency available to the caller.
  p.stop();
  assert(!p.running() && p.resume_hz() == 0 && !p.tick(6000, true, true, &h));
  assert(s.resume_hz() == 98700000);

  // Presets are well-formed ranges.
  for (uint8_t i = 0; i < kSweepPresetCount; ++i) {
    const SweepPreset& pr = sweep_preset(i);
    assert(pr.name && pr.config.end_hz > pr.config.start_hz && pr.config.step_hz && pr.config.dwell_ms);
  }

  // The journal labels a sweep as generated and carries its parameters and gap accounting.
  Event e{};
  e.sequence = 7; e.monotonic_ms = 1234; e.unix_time = 1792437813;
  e.source = Source::sweep; e.kind = Kind::sweep_stop; e.frequency_hz = 100200000;
  e.sweep_end_hz = 107900000; e.sweep_step_hz = 200000; e.sweep_dwell_ms = 500; e.sweep_settle_ms = kSweepSettleMs;
  e.sweep_hops = 12; e.sweep_unobserved_ms = 3100;
  char line[320];
  const size_t len = encode_event_jsonl(e, line, sizeof(line));
  assert(len > 0 && len < sizeof(line));
  const std::string text(line, len);
  assert(text.find("\"source\":\"GENERATED\"") != std::string::npos);
  assert(text.find("\"kind\":\"SWEEP STOP\"") != std::string::npos);
  assert(text.find("\"hops\":12") != std::string::npos && text.find("\"unobserved_ms\":3100") != std::string::npos);
  assert(text.find("\"settle_ms_assumed\":250") != std::string::npos && text.back() == '\n');

  // The model refuses to log anything else through the sweep door.
  EventModel model; model.reset();
  Event bad{}; bad.source = Source::rf; bad.kind = Kind::sweep_start;
  assert(!model.mark_sweep(bad));
  Event ok{}; ok.source = Source::sweep; ok.kind = Kind::sweep_start;
  assert(model.mark_sweep(ok) && model.count() == 1);
  std::puts("Dead Air sweep tests PASS");
}
