// Deterministic host test: g++ -std=c++17 -Wall -Wextra -Werror
// -Iapps/orcsdr-tab5/ui tests/dead_air_model_tests.cpp
// apps/orcsdr-tab5/ui/dead_air_model.cpp -o dead_air_model_tests
#include "dead_air_model.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
using namespace orcsdr::dead_air;

int main() {
  assert(reading_status({}) == Status::unavailable);
  Reading r{}; r.present = true;
  assert(reading_status(r) == Status::warming_up);
  r.valid = true; r.value = 12.0f;
  assert(reading_status(r) == Status::current);
  r.age_ms = 10001;
  assert(reading_status(r) == Status::stale);
  r.age_ms = 0; r.value = NAN;
  assert(reading_status(r) == Status::stale);
  assert(rf_rise_threshold_db(Sensitivity::quiet) == 12);
  assert(rf_rise_threshold_db(Sensitivity::standard) == 8);
  assert(rf_rise_threshold_db(Sensitivity::sensitive) == 5);
  EventModel model;
  model.reset();
  RfObservation s{}; s.frequency_hz = 100700000; s.configuration_revision = 1;
  s.valid = s.settled = true; s.level_dbfs = -78;
  for (int i = 0; i < 12; ++i) { s.monotonic_ms = i * 100; model.observe_rf(s); }
  assert(model.baseline_ready());
  assert(model.count() == 0);
  s.level_dbfs = -65; // +13 dB, legitimate candidate
  s.monotonic_ms = 1300; model.observe_rf(s);
  s.monotonic_ms = 1400; model.observe_rf(s);
  assert(model.count() == 0); // persistence required
  s.monotonic_ms = 1510; model.observe_rf(s);
  assert(model.count() == 1);
  assert(model.newest(0)->frequency_hz == 100700000);
  assert(model.newest(0)->value == -65);
  assert(model.newest(0)->reference == -78);
  assert(model.newest(0)->threshold == 8);
  s.monotonic_ms = 2000; model.observe_rf(s);
  assert(model.count() == 1); // no repeats during latch
  s.frequency_hz = 101100000; s.monotonic_ms = 2100; model.observe_rf(s);
  assert(!model.baseline_ready()); // retune resets baseline, no fake event
  s.valid = false; model.observe_rf(s); assert(model.count() == 1);
  model.mark_manual(2200, 0, 101100000);
  assert(model.count() == 2);
  assert(model.newest(0)->source == Source::manual);
  assert(!model.append_measured(Event{})); // reject invalid/manual classification
  for (unsigned i = 0; i < 60; ++i) model.mark_manual(3000 + i, 0, 0);
  assert(model.count() == kEventCapacity);
  assert(model.total() == 62);
  assert(model.newest(0)->sequence == 62);
  assert(model.newest(kEventCapacity) == nullptr);
  // A stall must invalidate the learned baseline and pending event.
  model.reset();
  s = {};
  s.frequency_hz = 101500000;
  s.configuration_revision = 2;
  s.valid = s.settled = true;
  s.level_dbfs = -85.0f;
  for (int i = 0; i < 12; ++i) {
    s.monotonic_ms = 10000 + i * 100;
    model.observe_rf(s);
  }
  assert(model.baseline_ready());
  s.level_dbfs = -60.0f;
  s.monotonic_ms = 12200; // 1100ms gap since last sample
  model.observe_rf(s);
  assert(!model.baseline_ready());
  assert(model.count() == 0);
  // A backward timestamp must not complete a pending candidate.
  s.monotonic_ms = 12100;
  model.observe_rf(s);
  assert(!model.baseline_ready());
  assert(model.self_check());
  puts("Dead Air model tests PASS");
}
