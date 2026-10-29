#include <cstdio>
#include <cstdlib>

#include "ft8_clock_estimator.hpp"

namespace {

[[noreturn]] void fail(const char* expression, int line) {
  std::fprintf(stderr, "FAIL line=%d check=%s\n", line, expression);
  std::exit(1);
}
#define CHECK(expression) do { if (!(expression)) fail(#expression, __LINE__); } while (false)

using namespace orcsdr::ft8::clock_dt;

const char* kCalls[] = {"K1ABC", "W2DEF", "N3GHI", "VE4JKL", "K5MNO", "W6PQR", "JA7STU", "DL8VWX", "G9YZA", "F0BCD"};

// One slot of stations whose DTs scatter around `centre`.
void add(Estimator& est, int centre, size_t stations, int jitter = 120) {
  Sample s[10];
  const int offsets[10] = {0, 40, -60, 90, -110, 20, -30, 70, -90, 10};
  for (size_t i = 0; i < stations; ++i)
    s[i] = Sample{static_cast<int16_t>(centre + offsets[i] * jitter / 120), station_id(kCalls[i])};
  est.add_slot(s, stations);
}

void test_fast_and_slow_clock() {
  Estimator est;
  add(est, 2000, 8);   // our clock is 2 s fast: every station appears 2 s late
  Estimate e = est.estimate();
  CHECK(e.valid && e.error_ms >= 1950 - kReceiverBiasMs && e.error_ms <= 2050 - kReceiverBiasMs);   // the receiver bias is subtracted
  CHECK(est.step_ms() <= -(1950 - kReceiverBiasMs) && est.step_ms() >= -(2050 - kReceiverBiasMs));

  est.clear();
  add(est, -1500, 7);  // slow clock
  CHECK(est.estimate().valid);
  CHECK(est.step_ms() >= 1450 + kReceiverBiasMs && est.step_ms() <= 1550 + kReceiverBiasMs);
}

void test_good_clock_is_left_alone() {
  Estimator est;
  add(est, 100, 8);
  add(est, -50, 8);
  CHECK(est.estimate().valid);
  CHECK(std::abs(est.estimate().error_ms) < kStepThresholdMs);
  CHECK(est.step_ms() == 0);
}

void test_not_enough_evidence() {
  Estimator est;
  add(est, 2000, 5);   // under the sample minimum
  CHECK(!est.estimate().valid && est.step_ms() == 0);
  Sample chatty[8];
  for (auto& c : chatty) c = Sample{2000, station_id("K1ABC")};
  est.clear();
  est.add_slot(chatty, 8);   // plenty of decodes but one station
  CHECK(!est.estimate().valid && est.step_ms() == 0);
  est.clear();
  est.add_slot(nullptr, 0);
  CHECK(!est.estimate().valid);
}

void test_one_vote_per_station() {
  Estimator est;
  Sample s[10];
  size_t n = 0;
  for (int i = 0; i < 7; ++i) s[n++] = Sample{static_cast<int16_t>(2000 + i * 10), station_id("K1ABC")};   // one chatty station, 2 s late
  s[n++] = Sample{20, station_id("W2DEF")};
  s[n++] = Sample{-40, station_id("N3GHI")};
  s[n++] = Sample{60, station_id("VE4JKL")};
  est.add_slot(s, n);
  CHECK(est.estimate().valid && est.estimate().stations == 4 && est.estimate().count == 10);
  CHECK(est.step_ms() == 0);   // three stations say the clock is right; the chatty one does not outvote them
}

void test_scattered_stations_are_not_believed() {
  Estimator est;
  Sample s[8];
  const int16_t dts[8] = {-1800, 1500, -200, 2200, 900, -1200, 1900, 300};
  for (size_t i = 0; i < 8; ++i) s[i] = Sample{dts[i], station_id(kCalls[i])};
  est.add_slot(s, 8);
  CHECK(!est.estimate().valid && est.step_ms() == 0);
}

void test_implausible_step_is_refused() {
  Estimator est;
  add(est, 9000, 8);
  CHECK(est.estimate().valid);
  CHECK(est.step_ms() == 0);
}

void test_window_ages_out() {
  Estimator est;
  add(est, 2000, 8);
  for (size_t i = 0; i < kWindowSlots; ++i) est.add_slot(nullptr, 0);   // quiet slots push the old evidence out
  CHECK(est.estimate().count == 0 && est.step_ms() == 0);

  // Pooling across slots: three stations in each of three slots reach the minimum together.
  est.clear();
  for (int slot = 0; slot < 3; ++slot) add(est, -1400, 3 + slot);
  CHECK(est.estimate().valid && est.step_ms() > 1300);
}

void test_station_id() {
  CHECK(station_id("K1ABC") == station_id("K1ABC"));
  CHECK(station_id("K1ABC") != station_id("K1ABD"));
  CHECK(station_id(nullptr) == station_id(""));
}

}  // namespace

int main() {
  CHECK(self_check());
  test_fast_and_slow_clock();
  test_good_clock_is_left_alone();
  test_not_enough_evidence();
  test_one_vote_per_station();
  test_scattered_stations_are_not_believed();
  test_implausible_step_is_refused();
  test_window_ages_out();
  test_station_id();
  std::puts("ft8_clock_estimator_tests: all passed");
  return 0;
}
