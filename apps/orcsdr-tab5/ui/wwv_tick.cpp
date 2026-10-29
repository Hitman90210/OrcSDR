#include "wwv_tick.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace orcsdr::wwv {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr size_t kDecim = 4;
constexpr float kCarrierAlpha = 0.01f;     // carrier phase loop: about 5 Hz wide at 3 kS/s
constexpr size_t kBox = 15;                // 5 ms at 3 kS/s: the matched filter of the 5 ms tick

float goertzel_power(const int16_t* x, size_t n, double hz) {
  const double w = 2.0 * kPi * hz / kSampleRate;
  const float coeff = static_cast<float>(2.0 * std::cos(w));
  float s1 = 0.0f, s2 = 0.0f;
  for (size_t i = 0; i < n; ++i) {
    const float s0 = static_cast<float>(x[i]) + coeff * s1 - s2;
    s2 = s1;
    s1 = s0;
  }
  return s1 * s1 + s2 * s2 - coeff * s1 * s2;
}

// Median and robust spread (1.4826 x median absolute deviation) of `v`, ignoring the bins within `skip` of `peak` (circular).
void robust_noise(const float* v, float* scratch, size_t peak, size_t skip, float* median, float* sigma) {
  size_t n = 0;
  for (size_t i = 0; i < kFoldBins; ++i) {
    const size_t d = i > peak ? i - peak : peak - i;
    const size_t circ = std::min(d, kFoldBins - d);
    if (circ > skip) scratch[n++] = v[i];
  }
  std::nth_element(scratch, scratch + n / 2, scratch + n);
  *median = scratch[n / 2];
  for (size_t i = 0; i < n; ++i) scratch[i] = std::fabs(scratch[i] - *median);
  std::nth_element(scratch, scratch + n / 2, scratch + n);
  *sigma = 1.4826f * scratch[n / 2];
}

size_t argmax(const float* v) {
  size_t k = 0;
  for (size_t i = 1; i < kFoldBins; ++i)
    if (v[i] > v[k]) k = i;
  return k;
}

float bin_to_start_ms(size_t bin) {
  // The matched filter peaks when the 5 ms box ends with the burst: the burst started kBox-1 samples (about 4.7 ms) earlier. The low-pass
  // filter delays everything by half its length (64 samples = 5.33 ms); take that out too, so the result is where the burst started in the
  // audio stream. (The receiver's own filters before the stream, the propagation and the fixed offset between stream and wall clock remain.)
  const float delay_ms = static_cast<float>((Detector::kTaps - 1) / 2) * 1000.0f / kSampleRate;
  const size_t box_start = (bin + kFoldBins - (kBox - 1)) % kFoldBins;
  float ms = static_cast<float>(box_start) / 3.0f - delay_ms;
  if (ms < 0.0f) ms += 1000.0f;
  return ms;
}

}  // namespace

Detector::Detector() { reset(0); }

void Detector::reset(uint32_t stream_offset) {
  state_ = State::acquiring;
  offset_ = stream_offset % kSampleRate;
  fed_ = 0;
  acquired_ = 0;
  carrier_hz_ = 0.0f;
  carrier_snr_db_ = 0.0f;
  phase_ = 0.0;
  // Low-pass for the complex baseband: windowed sinc, cut-off 1400 Hz, normalised to unity gain at DC.
  const double cutoff = 1400.0 / kSampleRate;
  const double mid = (kTaps - 1) / 2.0;
  double sum = 0.0;
  double tmp[kTaps];
  for (size_t i = 0; i < kTaps; ++i) {
    const double m = static_cast<double>(i) - mid;
    const double sinc = m == 0.0 ? 2.0 * cutoff : std::sin(2.0 * kPi * cutoff * m) / (kPi * m);
    tmp[i] = sinc * (0.54 - 0.46 * std::cos(2.0 * kPi * static_cast<double>(i) / (kTaps - 1)));
    sum += tmp[i];
  }
  for (size_t i = 0; i < kTaps; ++i) taps_[i] = static_cast<float>(tmp[i] / sum);
  std::memset(ring_re_, 0, sizeof(ring_re_));
  std::memset(ring_im_, 0, sizeof(ring_im_));
  ring_pos_ = 0;
  decim_ = 0;
  carrier_re_ = carrier_im_ = 0.0f;
  carrier_primed_ = false;
  mix_phase_ = 0;
  std::memset(box_re_, 0, sizeof(box_re_));
  std::memset(box_im_, 0, sizeof(box_im_));
  box_pos_ = 0;
  sum_re_ = sum_im_ = 0.0f;
  dec_count_ = 0;
  std::memset(row_, 0, sizeof(row_));
  std::memset(fold_all_, 0, sizeof(fold_all_));
  std::memset(fold_a_, 0, sizeof(fold_a_));
  std::memset(fold_b_, 0, sizeof(fold_b_));
  seconds_ = 0;
  last_bin_ = -1;
  row_started_ = false;
}

void Detector::acquire() {
  // Coarse scan of 900-1100 Hz in 1 Hz steps, then 0.1 Hz steps around the best line.
  float best = -1.0f;
  double best_hz = 1000.0;
  float powers[201];
  for (int i = 0; i <= 200; ++i) {
    powers[i] = goertzel_power(acquire_, kAcquireSamples, 900.0 + i);
    if (powers[i] > best) {
      best = powers[i];
      best_hz = 900.0 + i;
    }
  }
  float sorted[201];
  std::memcpy(sorted, powers, sizeof(sorted));
  std::nth_element(sorted, sorted + 100, sorted + 201);
  const float median = std::max(sorted[100], 1e-6f);
  float fine_best = -1.0f;
  double fine_hz = best_hz;
  for (int i = -10; i <= 10; ++i) {
    const double hz = best_hz + 0.1 * i;
    const float p = goertzel_power(acquire_, kAcquireSamples, hz);
    if (p > fine_best) {
      fine_best = p;
      fine_hz = hz;
    }
  }
  carrier_hz_ = static_cast<float>(fine_hz);
  carrier_snr_db_ = 10.0f * std::log10(std::max(fine_best, best) / median);
  state_ = carrier_snr_db_ >= kMinCarrierSnrDb ? State::tracking : State::failed;
}

void Detector::finish_second() {
  // Atmospheric crashes and interference put a huge burst into one second; folded as it is, a single one outweighs a minute of weak ticks.
  // Above 10 x that second's median power (noise alone rarely exceeds 8 x its mean) a bin is compressed logarithmically: a crash 1e6 times
  // stronger counts about 15 times the limit instead of a million, and the order of the bins is kept (a hard clip would flatten the top
  // of a strong tick and move its peak).
  std::memcpy(scratch_, row_, sizeof(row_));
  std::nth_element(scratch_, scratch_ + kFoldBins / 2, scratch_ + kFoldBins);
  const float limit = 10.0f * std::max(scratch_[kFoldBins / 2], 1e-9f);
  for (size_t i = 0; i < kFoldBins; ++i)
    if (row_[i] > limit) row_[i] = limit * (1.0f + std::log(row_[i] / limit));
  for (size_t i = 0; i < kFoldBins; ++i) {
    fold_all_[i] += row_[i];
    ((seconds_ & 1u) ? fold_b_ : fold_a_)[i] += row_[i];
  }
  ++seconds_;
  // Rebuild the running box sum exactly once a second so float rounding cannot accumulate.
  float re = 0.0f, im = 0.0f;
  for (size_t i = 0; i < kBox; ++i) {
    re += box_re_[i];
    im += box_im_[i];
  }
  sum_re_ = re;
  sum_im_ = im;
}

void Detector::run(const int16_t* samples, size_t count) {
  static const float kCos[3] = {1.0f, -0.5f, -0.5f};
  static const float kSin[3] = {0.0f, 0.8660254f, -0.8660254f};
  const double step = 2.0 * kPi * carrier_hz_ / kSampleRate;
  for (size_t n = 0; n < count; ++n) {
    const uint32_t index = static_cast<uint32_t>((offset_ + fed_) % kSampleRate);
    ++fed_;
    phase_ += step;
    if (phase_ > 2.0 * kPi) phase_ -= 2.0 * kPi;
    const float x = static_cast<float>(samples[n]);
    ring_re_[ring_pos_] = x * static_cast<float>(std::cos(phase_));
    ring_im_[ring_pos_] = -x * static_cast<float>(std::sin(phase_));
    ring_pos_ = (ring_pos_ + 1) % kTaps;
    if (++decim_ < kDecim) continue;
    decim_ = 0;

    float zr = 0.0f, zi = 0.0f;
    for (size_t k = 0; k < kTaps; ++k) {
      const size_t at = (ring_pos_ + kTaps - 1 - k) % kTaps;
      zr += taps_[k] * ring_re_[at];
      zi += taps_[k] * ring_im_[at];
    }
    if (!carrier_primed_) {
      carrier_re_ = zr;
      carrier_im_ = zi;
      carrier_primed_ = true;
    }
    carrier_re_ += kCarrierAlpha * (zr - carrier_re_);
    carrier_im_ += kCarrierAlpha * (zi - carrier_im_);
    const float mag = std::sqrt(carrier_re_ * carrier_re_ + carrier_im_ * carrier_im_) + 1e-9f;
    const float y = (zr * carrier_re_ + zi * carrier_im_) / mag;   // real part of zb x conj(carrier) / |carrier|: the demodulated audio

    const float ur = y * kCos[mix_phase_];
    const float ui = -y * kSin[mix_phase_];
    mix_phase_ = static_cast<uint8_t>((mix_phase_ + 1) % 3);
    sum_re_ += ur - box_re_[box_pos_];
    sum_im_ += ui - box_im_[box_pos_];
    box_re_[box_pos_] = ur;
    box_im_[box_pos_] = ui;
    box_pos_ = (box_pos_ + 1) % kBox;
    const float power = sum_re_ * sum_re_ + sum_im_ * sum_im_;

    const int32_t bin = static_cast<int32_t>(index / kDecim);
    if (bin < last_bin_) {            // the stream's second rolled over
      if (row_started_ && seconds_ < kMaxSeconds) finish_second();
      row_started_ = true;
      std::memset(row_, 0, sizeof(row_));
    }
    last_bin_ = bin;
    if (row_started_) row_[bin] = power;
    ++dec_count_;
  }
}

void Detector::process(const int16_t* samples, size_t count) {
  if (state_ == State::failed) return;
  if (state_ == State::acquiring) {
    const size_t take = std::min(count, kAcquireSamples - acquired_);
    std::memcpy(acquire_ + acquired_, samples, take * sizeof(int16_t));
    acquired_ += take;
    if (acquired_ < kAcquireSamples) return;
    acquire();
    if (state_ == State::failed) return;
    // Replay the acquisition buffer through the tracking pipeline so no audio is lost, then the rest of this block.
    fed_ = 0;
    run(acquire_, kAcquireSamples);
    run(samples + take, count - take);
    return;
  }
  run(samples, count);
}

Result Detector::result() const {
  Result r;
  r.carrier_found = state_ == State::tracking;
  r.carrier_hz = carrier_hz_;
  r.carrier_snr_db = carrier_snr_db_;
  r.seconds = seconds_;
  if (!r.carrier_found || seconds_ < 2) return r;

  const size_t peak = argmax(fold_all_);
  float median = 0.0f, sigma = 0.0f;
  robust_noise(fold_all_, scratch_, peak, 30, &median, &sigma);
  r.peak_z = sigma > 0.0f ? (fold_all_[peak] - median) / sigma : 0.0f;
  r.tick_ms = bin_to_start_ms(peak);
  const size_t pa = argmax(fold_a_);
  const size_t pb = argmax(fold_b_);
  r.even_ms = bin_to_start_ms(pa);
  r.odd_ms = bin_to_start_ms(pb);
  const size_t d = pa > pb ? pa - pb : pb - pa;
  r.halves_agree = std::min(d, kFoldBins - d) <= 6;   // 2 ms
  r.ticks = seconds_ >= kMinSeconds && r.peak_z >= kMinPeakZ && r.halves_agree;
  return r;
}

bool self_check() {
  Detector* d = new Detector();
  const Result r = d->result();
  const bool ok = !r.carrier_found && !r.ticks && r.seconds == 0;
  delete d;
  return ok;
}

}  // namespace orcsdr::wwv
