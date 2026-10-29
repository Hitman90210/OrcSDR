#include "dead_air_audio.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>

using namespace orcsdr::dead_air;

static std::vector<int16_t> tone(size_t n, int amplitude) {
  std::vector<int16_t> v(n);
  for (size_t i = 0; i < n; ++i) v[i] = static_cast<int16_t>(std::lround(amplitude * std::sin(0.1 * static_cast<double>(i))));
  return v;
}

int main() {
  // Silence is finite and at the floor; full scale square is 0 dBFS.
  assert(amplitude_to_dbfs(0.0f) == kAudioSilenceDbfs);
  assert(amplitude_to_dbfs(-1.0f) == kAudioSilenceDbfs);
  assert(std::fabs(amplitude_to_dbfs(1.0f)) < 1e-4f);
  assert(std::fabs(amplitude_to_dbfs(0.5f) - (-6.0206f)) < 0.01f);

  AudioMeter m;
  m.configure(48000);
  AudioBucket out[8];
  // 3840 samples make exactly one bucket; leftovers carry over.
  std::vector<int16_t> sil(3840, 0);
  assert(m.feed(sil.data(), sil.size(), out, 8) == 1 && out[0].rms_dbfs == kAudioSilenceDbfs && out[0].peak_dbfs == kAudioSilenceDbfs);
  assert(m.feed(sil.data(), 1000, out, 8) == 0);
  assert(m.feed(sil.data(), 2840, out, 8) == 1);
  // A half-scale square wave: RMS = peak = -6.02 dBFS.
  std::vector<int16_t> sq(3840);
  for (size_t i = 0; i < sq.size(); ++i) sq[i] = i % 2 ? 16384 : -16384;
  assert(m.feed(sq.data(), sq.size(), out, 8) == 1);
  assert(std::fabs(out[0].rms_dbfs + 6.02f) < 0.05f && std::fabs(out[0].peak_dbfs + 6.02f) < 0.05f);
  // A sine peaks 3 dB above its RMS.
  const auto s = tone(3840, 20000);
  assert(m.feed(s.data(), s.size(), out, 8) == 1);
  assert(std::fabs((out[0].peak_dbfs - out[0].rms_dbfs) - 3.01f) < 0.2f);
  // More buckets than the caller has room for are counted, not written past the end.
  std::vector<int16_t> many(3840 * 5, 1000);
  assert(m.feed(many.data(), many.size(), out, 2) == 2);
  assert(m.feed(nullptr, 10, out, 2) == 0);

  // Detector: warm-up, then a sustained rise fires once, with cooldown.
  AudioDetector d;
  AudioFinding f;
  uint32_t t = 1000;
  const Sensitivity std_sens = Sensitivity::standard;  // +8 dB
  for (uint32_t i = 0; i < AudioDetector::kWarmupBuckets; ++i, t += 80) {
    assert(!d.observe({-40.0f, -30.0f}, t, 1, std_sens, &f));
    assert(d.baseline_ready() == (i + 1 >= AudioDetector::kWarmupBuckets));
  }
  assert(std::fabs(d.baseline() + 40.0f) < 0.01f);
  // A short blip (2 buckets) does not fire.
  assert(!d.observe({-25.0f, -20.0f}, t, 1, std_sens, &f)); t += 80;
  assert(!d.observe({-25.0f, -20.0f}, t, 1, std_sens, &f)); t += 80;
  assert(!d.observe({-40.0f, -30.0f}, t, 1, std_sens, &f)); t += 80;
  // Three buckets above +8 dB fire exactly once.
  assert(!d.observe({-28.0f, -20.0f}, t, 1, std_sens, &f)); t += 80;
  assert(!d.observe({-28.0f, -20.0f}, t, 1, std_sens, &f)); t += 80;
  assert(d.observe({-28.0f, -20.0f}, t, 1, std_sens, &f)); t += 80;
  assert(std::fabs(f.rms_dbfs + 28.0f) < 0.01f && std::fabs(f.baseline_dbfs + 40.0f) < 0.01f && f.threshold_db == 8.0f);
  assert(!d.observe({-28.0f, -20.0f}, t, 1, std_sens, &f)); t += 80;  // latched
  // Quiet again, then another rise inside the cooldown is not reported.
  for (int i = 0; i < 4; ++i, t += 80) assert(!d.observe({-40.0f, -30.0f}, t, 1, std_sens, &f));
  for (int i = 0; i < 3; ++i, t += 80) { const bool fired = d.observe({-28.0f, -20.0f}, t, 1, std_sens, &f); assert(!fired); }

  // A configuration change (retune, mode, gain) resets the baseline: no event can come from the step itself.
  AudioDetector e;
  t = 5000;
  for (uint32_t i = 0; i < AudioDetector::kWarmupBuckets; ++i, t += 80) e.observe({-45.0f, -35.0f}, t, 1, std_sens, &f);
  for (int i = 0; i < 5; ++i, t += 80) assert(!e.observe({-20.0f, -10.0f}, t, 2, std_sens, &f));
  assert(!e.baseline_ready());
  // A gap in the audio resets it too.
  AudioDetector g;
  t = 9000;
  for (uint32_t i = 0; i < AudioDetector::kWarmupBuckets; ++i, t += 80) g.observe({-45.0f, -35.0f}, t, 1, std_sens, &f);
  assert(g.baseline_ready());
  t += 1000;
  assert(!g.observe({-20.0f, -10.0f}, t, 1, std_sens, &f) && !g.baseline_ready());
  // Silence never builds a baseline and never fires.
  AudioDetector z;
  t = 100;
  for (int i = 0; i < 60; ++i, t += 80) assert(!z.observe({kAudioSilenceDbfs, kAudioSilenceDbfs}, t, 1, std_sens, &f));
  assert(!z.baseline_ready());
  // Non-finite input resets instead of poisoning the baseline.
  assert(!z.observe({NAN, NAN}, t, 1, std_sens, &f));
  // Sensitivity changes the threshold.
  AudioDetector h;
  t = 100;
  for (uint32_t i = 0; i < AudioDetector::kWarmupBuckets; ++i, t += 80) h.observe({-40.0f, -30.0f}, t, 1, Sensitivity::sensitive, &f);
  bool fired = false;
  for (int i = 0; i < 3; ++i, t += 80) fired = h.observe({-34.0f, -30.0f}, t, 1, Sensitivity::sensitive, &f);  // +6 dB > +5
  assert(fired && f.threshold_db == 5.0f);
  std::puts("Dead Air audio tests PASS");
}
