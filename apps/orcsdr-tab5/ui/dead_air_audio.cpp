#include "dead_air_audio.hpp"

#include <algorithm>
#include <cmath>

namespace orcsdr::dead_air {

float amplitude_to_dbfs(float fraction) {
  if (!(fraction > 0.0f) || !std::isfinite(fraction)) return kAudioSilenceDbfs;
  return std::max(kAudioSilenceDbfs, 20.0f * std::log10(fraction));
}

void AudioMeter::configure(uint32_t sample_rate_sps) {
  samples_per_bucket_ = std::max<uint32_t>(1, sample_rate_sps / 1000u * kAudioBucketMs);
  reset();
}

void AudioMeter::reset() {
  filled_ = 0;
  sum_sq_ = 0.0f;
  peak_ = 0;
}

size_t AudioMeter::feed(const int16_t* pcm, size_t count, AudioBucket* out, size_t max_out) {
  if (!pcm) return 0;
  size_t emitted = 0;
  for (size_t i = 0; i < count; ++i) {
    const int32_t v = pcm[i];
    sum_sq_ += static_cast<float>(v) * static_cast<float>(v);
    const int32_t a = v < 0 ? -v : v;
    if (a > peak_) peak_ = a;
    if (++filled_ >= samples_per_bucket_) {
      if (out && emitted < max_out) {
        const float rms = std::sqrt(sum_sq_ / static_cast<float>(filled_)) / 32768.0f;
        out[emitted].rms_dbfs = amplitude_to_dbfs(rms);
        out[emitted].peak_dbfs = amplitude_to_dbfs(static_cast<float>(peak_) / 32768.0f);
        ++emitted;
      }
      filled_ = 0;
      sum_sq_ = 0.0f;
      peak_ = 0;
    }
  }
  return emitted;
}

void AudioDetector::reset() {
  count_ = above_ = 0;
  baseline_ = 0.0f;
  have_ = latched_ = false;
}

bool AudioDetector::observe(const AudioBucket& b, uint32_t now_ms, uint32_t revision, Sensitivity sensitivity,
                            AudioFinding* out) {
  if (!std::isfinite(b.rms_dbfs)) { reset(); return false; }
  if (have_ && (revision != revision_ || now_ms - last_ms_ > kMaxGapMs)) reset();
  revision_ = revision;
  last_ms_ = now_ms;
  have_ = true;
  if (b.rms_dbfs < kMinLevelDbfs) {  // silence is not a baseline and not activity
    above_ = 0;
    latched_ = false;
    return false;
  }
  if (!baseline_ready()) {
    baseline_ = count_ == 0 ? b.rms_dbfs : baseline_ + (b.rms_dbfs - baseline_) / static_cast<float>(count_ + 1);
    ++count_;
    return false;
  }
  const float threshold = rf_rise_threshold_db(sensitivity);
  const float excess = b.rms_dbfs - baseline_;
  if (excess < threshold) {
    above_ = 0;
    if (excess <= threshold * 0.5f) latched_ = false;
    if (excess < threshold * 0.5f) baseline_ += (b.rms_dbfs - baseline_) * 0.05f;  // follow slow drift only
    return false;
  }
  if (++above_ < kPersistBuckets || latched_ || now_ms < cooldown_until_) return false;
  latched_ = true;
  cooldown_until_ = now_ms + kCooldownMs;
  if (out) {
    out->rms_dbfs = b.rms_dbfs;
    out->baseline_dbfs = baseline_;
    out->threshold_db = threshold;
  }
  return true;
}

}  // namespace orcsdr::dead_air
