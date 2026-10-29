#pragma once
// Level metering and a conservative activity detector for the receiver's demodulated audio. This measures how loud
// the audio is, nothing more: it does not recognise speech, words or sources. Pure logic, host-tested. The meter runs
// on the DSP thread (no allocation, no locks); the detector runs on the UI task.
#include <cstddef>
#include <cstdint>

#include "dead_air_model.hpp"

namespace orcsdr::dead_air {

constexpr uint32_t kAudioBucketMs = 80;       // one level reading per 80 ms of audio
constexpr float kAudioSilenceDbfs = -100.0f;  // reported for digital silence, so levels are always finite

struct AudioBucket {
  float rms_dbfs = kAudioSilenceDbfs;
  float peak_dbfs = kAudioSilenceDbfs;
};

float amplitude_to_dbfs(float fraction_of_full_scale);

// Accumulates 16-bit mono PCM and emits one bucket per kAudioBucketMs.
class AudioMeter {
 public:
  void configure(uint32_t sample_rate_sps);
  void reset();
  // Returns the number of buckets written to `out` (at most `max_out`); samples beyond that are still counted.
  size_t feed(const int16_t* pcm, size_t count, AudioBucket* out, size_t max_out);
 private:
  uint32_t samples_per_bucket_ = 3840;  // 48 kHz
  uint32_t filled_ = 0;
  float sum_sq_ = 0.0f;
  int32_t peak_ = 0;
};

struct AudioFinding {
  float rms_dbfs = 0.0f;
  float baseline_dbfs = 0.0f;
  float threshold_db = 0.0f;
};

// Fires when the RMS level stays at least the sensitivity threshold above an adaptive baseline for a few buckets.
// A configuration change, a gap in the audio or silence resets it, so a retune can never look like activity.
class AudioDetector {
 public:
  static constexpr uint32_t kWarmupBuckets = 30;      // 2.4 s
  static constexpr uint32_t kPersistBuckets = 3;      // 240 ms above threshold
  static constexpr uint32_t kCooldownMs = 3000;
  static constexpr uint32_t kMaxGapMs = 400;
  static constexpr float kMinLevelDbfs = -70.0f;      // below this the audio is effectively silent
  void reset();
  bool observe(const AudioBucket& bucket, uint32_t now_ms, uint32_t revision, Sensitivity sensitivity, AudioFinding* out);
  bool baseline_ready() const { return count_ >= kWarmupBuckets; }
  float baseline() const { return baseline_; }
 private:
  uint32_t revision_ = 0, last_ms_ = 0, cooldown_until_ = 0;
  uint32_t count_ = 0, above_ = 0;
  float baseline_ = 0.0f;
  bool have_ = false, latched_ = false;
};

}  // namespace orcsdr::dead_air
