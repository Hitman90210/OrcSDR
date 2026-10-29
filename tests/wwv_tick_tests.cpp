#include "wwv_tick.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <random>
#include <vector>

using namespace orcsdr::wwv;

namespace {

constexpr uint32_t kMinSecondsForVerdict = Detector::kMinSeconds;

[[noreturn]] void fail(const char* expression, int line) {
  std::fprintf(stderr, "FAIL line=%d check=%s\n", line, expression);
  std::exit(1);
}
#define CHECK(expression) do { if (!(expression)) fail(#expression, __LINE__); } while (false)

constexpr double kPi = 3.14159265358979323846;

// WWV as the receiver hears it when tuned 1 kHz below the carrier (USB): the carrier is a line at `carrier_hz`; the 5 ms 1000 Hz tick is
// its upper sideband, a burst at carrier_hz + 1000 Hz of amplitude depth/2 (AM with depth `depth`). No tick in the 29th and 59th second.
std::vector<int16_t> make_signal(double seconds, double carrier_hz, double tick_start_ms, double depth, double noise_sigma, unsigned seed,
                                 bool with_ticks = true, double carrier_amp = 1000.0) {
  const size_t n = static_cast<size_t>(seconds * kSampleRate);
  std::mt19937 rng(seed);
  std::normal_distribution<double> noise(0.0, noise_sigma * carrier_amp);
  std::vector<int16_t> out(n);
  const size_t burst = kSampleRate * 5 / 1000;
  for (size_t i = 0; i < n; ++i) {
    const double t = static_cast<double>(i) / kSampleRate;
    double v = carrier_amp * std::cos(2.0 * kPi * carrier_hz * t) + noise(rng);
    const double second = std::floor(t);
    const double into = (t - second) * 1000.0 - tick_start_ms;
    if (with_ticks && static_cast<long>(second) % 30 != 29 && into >= 0.0 && into < 5.0 && i / kSampleRate < seconds)
      v += carrier_amp * depth / 2.0 * std::cos(2.0 * kPi * (carrier_hz + 1000.0) * t);
    out[i] = static_cast<int16_t>(std::max(-32767.0, std::min(32767.0, v)));
  }
  (void)burst;
  return out;
}

Result run(const std::vector<int16_t>& x, uint32_t offset = 0, size_t chunk = 4096) {
  auto d = std::make_unique<Detector>();
  d->reset(offset);
  for (size_t at = 0; at < x.size(); at += chunk) d->process(x.data() + at, std::min(chunk, x.size() - at));
  return d->result();
}

void show(const char* name, const Result& r) {
  std::printf("%s: carrier %d %.2f Hz %.1f dB, %u s, ticks=%d tick %.2f ms (even %.2f odd %.2f) z=%.1f agree=%d\n", name, r.carrier_found ? 1 : 0,
              static_cast<double>(r.carrier_hz), static_cast<double>(r.carrier_snr_db), static_cast<unsigned>(r.seconds), r.ticks ? 1 : 0,
              static_cast<double>(r.tick_ms), static_cast<double>(r.even_ms), static_cast<double>(r.odd_ms), static_cast<double>(r.peak_z),
              r.halves_agree ? 1 : 0);
}

float ms_distance(float a, float b) {
  float d = std::fabs(a - b);
  return std::min(d, 1000.0f - d);
}

void test_strong_ticks_are_found_in_the_right_place() {
  const Result r = run(make_signal(40, 998.3, 163.4, 0.8, 0.05, 1));
  show("strong", r);
  CHECK(r.carrier_found && std::fabs(r.carrier_hz - 998.3f) < 0.3f);
  CHECK(r.ticks && r.halves_agree);
  CHECK(ms_distance(r.tick_ms, 163.4f) < 1.2f);
  CHECK(r.seconds >= 35);
}

void test_weak_signal_needs_a_minute_but_is_found() {
  // Noise as strong as 0.8 x the carrier amplitude (the 10 MHz WWV carrier on the bench was about as strong as the noise), tick depth 0.8.
  // Measured limit of the detector (6 runs each): found in 60-120 s while the noise is no stronger than the carrier amplitude; at 0.5 depth
  // it needs about half that noise; at 1.6 x the carrier it sees nothing in 120 s (noise alone scores z of 3.5-4.2, the threshold is 7).
  const Result r = run(make_signal(75, 1001.7, 421.0, 0.8, 0.8, 3));
  show("weak", r);
  CHECK(r.carrier_found);
  CHECK(r.ticks);
  CHECK(ms_distance(r.tick_ms, 421.0f) < 1.5f);
  // Too weak for a verdict: the detector says so instead of guessing.
  const Result hopeless = run(make_signal(75, 1001.7, 421.0, 0.8, 3.0, 4));
  CHECK(hopeless.carrier_found && !hopeless.ticks);
}

void test_impulsive_noise_does_not_hide_or_fake_a_tick() {
  // Loud 20 ms crashes at random times (30 times the carrier amplitude, 12 in a minute): the ticks are still found, and crashes alone are not a tick.
  for (const bool with_ticks : {true, false}) {
    auto x = make_signal(60, 999.4, 333.0, 0.8, 0.7, 31, with_ticks);
    std::mt19937 rng(77);
    for (int k = 0; k < 12; ++k) {
      const size_t at = rng() % (x.size() - 300);
      for (size_t i = 0; i < 240; ++i) x[at + i] = static_cast<int16_t>((rng() & 1u) ? 30000 : -30000);
    }
    const Result r = run(x);
    show(with_ticks ? "impulsive+ticks" : "impulsive only", r);
    CHECK(r.carrier_found);
    if (with_ticks) {
      CHECK(r.ticks && ms_distance(r.tick_ms, 333.0f) < 1.5f);
    } else {
      CHECK(!r.ticks);
    }
  }
}

void test_noise_alone_never_claims_a_tick() {
  for (unsigned seed = 1; seed <= 12; ++seed) {
    const Result r = run(make_signal(45, 999.0, 100.0, 0.0, 1.58, 100 + seed, false));
    CHECK(!r.ticks);          // a carrier and noise but no tick
    const Result n = run(make_signal(45, 999.0, 100.0, 0.0, 1.0, 200 + seed, false, 0.0));   // noise only: no carrier at all
    CHECK(!n.carrier_found && !n.ticks);
  }
}

void test_block_size_and_stream_offset() {
  const auto x = make_signal(30, 998.0, 250.0, 0.8, 0.3, 5);
  const Result a = run(x, 0, 4096);
  const Result b = run(x, 0, 777);
  CHECK(a.ticks && b.ticks);
  CHECK(std::fabs(a.tick_ms - b.tick_ms) < 0.01f);      // the result does not depend on how the audio is cut into blocks
  // The stream's second boundary moves with the offset: starting 3000 samples (250 ms) later puts the tick 250 ms earlier in the second.
  const Result c = run(x, 3000, 4096);
  CHECK(c.ticks);
  CHECK(ms_distance(c.tick_ms, a.tick_ms + 250.0f) < 1.0f);
}

void test_short_or_missing_signal_is_not_a_result() {
  const Result few = run(make_signal(10, 999.0, 100.0, 0.8, 0.05, 9));
  CHECK(few.carrier_found && !few.ticks);     // too few seconds to claim a tick
  const Result none = run(std::vector<int16_t>(1000, 0));
  CHECK(!none.carrier_found && none.seconds == 0 && !none.ticks);
}

std::vector<int16_t> read_wav(const char* path) {
  std::FILE* f = std::fopen(path, "rb");
  if (f == nullptr) return {};
  std::fseek(f, 0, SEEK_END);
  const long size = std::ftell(f);
  std::fseek(f, 44, SEEK_SET);
  std::vector<int16_t> out(static_cast<size_t>(size - 44) / 2);
  const size_t got = std::fread(out.data(), 2, out.size(), f);
  std::fclose(f);
  out.resize(got);
  return out;
}

void test_real_capture(const char* path) {
  // A real WWV carrier on 10 MHz (Tab5, receiver 1 kHz below the carrier, 2026-10-23 08:30 UTC, 15 s). The carrier is strong; 15 s is too
  // short for a verdict, so only the carrier is asserted and the fold statistics are printed.
  const auto x = read_wav(path);
  if (x.size() < 100000) {
    std::printf("wwv_tick_tests: real capture %s not found, skipped\n", path);
    return;
  }
  const Result r = run(x);
  std::printf("real 10 MHz capture: carrier %.2f Hz (%.1f dB), %u seconds, peak z %.1f, tick at %.1f ms (even %.1f, odd %.1f), ticks=%d\n",
              static_cast<double>(r.carrier_hz), static_cast<double>(r.carrier_snr_db), static_cast<unsigned>(r.seconds),
              static_cast<double>(r.peak_z), static_cast<double>(r.tick_ms), static_cast<double>(r.even_ms), static_cast<double>(r.odd_ms),
              r.ticks ? 1 : 0);
  CHECK(r.carrier_found);
  CHECK(r.carrier_hz > 997.0f && r.carrier_hz < 999.0f);
  CHECK(r.carrier_snr_db > 15.0f);
  CHECK(r.seconds >= 10 && r.seconds < kMinSecondsForVerdict);   // too little audio for a verdict, by design
  // The tick is already there: both looks (even and odd seconds) agree to within a millisecond, far above the noise. The position is
  // where the burst started inside the audio stream's second: it includes the receiver's filters and propagation (about 32 ms here).
  CHECK(r.halves_agree && r.peak_z > 12.0f);
  CHECK(r.tick_ms > 28.0f && r.tick_ms < 36.0f);
}

}  // namespace

int main(int argc, char** argv) {
  CHECK(self_check());
  test_strong_ticks_are_found_in_the_right_place();
  test_weak_signal_needs_a_minute_but_is_found();
  test_impulsive_noise_does_not_hide_or_fake_a_tick();
  test_noise_alone_never_claims_a_tick();
  test_block_size_and_stream_offset();
  test_short_or_missing_signal_is_not_a_result();
  test_real_capture(argc > 1 ? argv[1] : "tests/fixtures/wwv/wwv_10mhz_dial_1khz_below_15s.wav");
  std::puts("wwv_tick_tests: all passed");
  return 0;
}
