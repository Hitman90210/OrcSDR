#pragma once

#include <cstddef>
#include <cstdint>

// Finds the once-per-second time-signal tick of WWV (Fort Collins) in the receiver's 12 kS/s USB audio, and says where inside the second it
// falls. Pure signal processing (no hardware, no allocation after construction), host-tested.
//
// WWV is AM: a carrier with a 5 ms burst of 1000 Hz at the start of every second (none in the 29th and 59th). The receiver is tuned
// 1 kHz BELOW the carrier, so in the USB audio the carrier is a steady line near 1000 Hz and the tick is its upper sideband near 2000 Hz.
// Tuned exactly on the carrier the carrier would be removed as DC and only the faint sideband would be left under the noise; keeping it
// lets the detector demodulate synchronously against it, which is far more sensitive:
//   1. acquire the carrier line (900-1100 Hz) from the first 2.7 s,
//   2. mix it to DC, low-pass to +-1.4 kHz and decimate by 4 (3 kS/s), track its phase with a ~5 Hz loop,
//   3. synchronous detection (real part against the carrier phase) gives the audio, the 1000 Hz tick is mixed to DC,
//   4. a 5 ms boxcar (the matched filter of a rectangular tone burst) gives the tick power,
//   5. the power is folded over one second (1/3 ms bins); a tick is claimed only if the even seconds and the odd seconds (two independent
//      looks at the same tick) put it in the same place and the fold peak is far above the fold's own noise.
// The position is reported against the audio stream's own second boundary (sample index modulo 12000); the caller adds the stream's
// offset to the wall clock. Fixed delays (the audio filters, the matched filter, propagation) are not removed here: the application
// calibrates them against a known-good clock.
namespace orcsdr::wwv {

constexpr uint32_t kSampleRate = 12000;
constexpr size_t kAcquireSamples = 32768;   // 2.73 s: carrier search resolution about 0.4 Hz
constexpr size_t kFoldBins = 3000;          // one second at 3 kS/s: 1/3 ms per bin
constexpr size_t kMaxSeconds = 240;

struct Result {
  bool carrier_found = false;
  float carrier_hz = 0.0f;
  float carrier_snr_db = 0.0f;   // carrier line over the median of the search band
  uint32_t seconds = 0;          // whole seconds folded so far
  bool ticks = false;            // a tick was found (see above)
  float tick_ms = 0.0f;          // where the tick starts inside the stream's second, 0..999.99 (valid when ticks)
  float peak_z = 0.0f;           // fold peak above the fold's own noise, in robust standard deviations
  float even_ms = 0.0f;          // the peak position found from the even seconds alone and the odd seconds alone, for inspection
  float odd_ms = 0.0f;
  bool halves_agree = false;     // the two independent looks put the tick in the same place (within 2 ms)
};

class Detector {
 public:
  Detector();
  // `stream_offset` = the audio stream's sample index of the first sample fed, modulo 12000 (so the fold lines up with the stream's seconds).
  void reset(uint32_t stream_offset);
  // Feed consecutive 12 kS/s samples. Safe to call with any block size.
  void process(const int16_t* samples, size_t count);
  Result result() const;
  bool carrier_found() const { return state_ == State::tracking; }
  bool failed() const { return state_ == State::failed; }   // acquisition finished and found no carrier line
  uint32_t seconds() const { return seconds_; }

  static constexpr size_t kTaps = 129;
  static constexpr float kMinCarrierSnrDb = 12.0f;   // below this the carrier line is not distinguishable from noise
  static constexpr float kMinPeakZ = 7.0f;           // fold peak, in robust standard deviations of the rest of the fold
  static constexpr uint32_t kMinSeconds = 20;

 private:
  enum class State : uint8_t { acquiring, tracking, failed };
  void acquire();
  void run(const int16_t* samples, size_t count);
  void finish_second();

  State state_ = State::acquiring;
  uint32_t offset_ = 0;
  uint64_t fed_ = 0;                       // samples fed in total
  int16_t acquire_[kAcquireSamples];
  size_t acquired_ = 0;
  float carrier_hz_ = 0.0f;
  float carrier_snr_db_ = 0.0f;

  // tracking pipeline
  double phase_ = 0.0;
  float taps_[kTaps];
  float ring_re_[kTaps];
  float ring_im_[kTaps];
  size_t ring_pos_ = 0;
  uint8_t decim_ = 0;
  float carrier_re_ = 0.0f;
  float carrier_im_ = 0.0f;
  bool carrier_primed_ = false;
  uint8_t mix_phase_ = 0;
  float box_re_[15];
  float box_im_[15];
  size_t box_pos_ = 0;
  float sum_re_ = 0.0f;
  float sum_im_ = 0.0f;

  // folding
  uint64_t dec_count_ = 0;                 // decimated samples produced
  float row_[kFoldBins];
  float fold_all_[kFoldBins];
  float fold_a_[kFoldBins];                // even seconds
  float fold_b_[kFoldBins];                // odd seconds: an independent look at the same tick
  mutable float scratch_[kFoldBins];
  uint32_t seconds_ = 0;
  int32_t last_bin_ = -1;
  bool row_started_ = false;
};

bool self_check();

}  // namespace orcsdr::wwv
