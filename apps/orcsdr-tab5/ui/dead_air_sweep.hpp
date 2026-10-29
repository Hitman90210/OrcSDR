#pragma once
// Generated tuning schedule for Dead Air. A sweep is NOT a demodulator and NOT simultaneous coverage: one tuner
// listens to one channel at a time, and each hop leaves an unobserved gap while the receiver settles. This module is
// pure logic (no radio, no display, no SD); the application feeds it the clock and the receiver's settled flag and
// performs the retunes it asks for.
#include <cstdint>

namespace orcsdr::dead_air {

enum class SweepPattern : uint8_t { forward, bounce };

struct SweepConfig {
  uint32_t start_hz = 0;
  uint32_t end_hz = 0;
  uint32_t step_hz = 0;
  uint32_t dwell_ms = 0;  // time spent listening on a channel AFTER it has settled
  SweepPattern pattern = SweepPattern::forward;
};

struct SweepPreset {
  const char* name;
  SweepConfig config;
};

constexpr uint8_t kSweepPresetCount = 2;
const SweepPreset& sweep_preset(uint8_t index);
constexpr uint16_t kSweepDwellChoicesMs[] = {250, 500, 1000, 2000};
constexpr uint8_t kSweepDwellCount = 4;
// Assumed wait after a hot retune before a channel counts as observed. This is a conservative assumption, not a
// measurement, and it is written to the journal as such.
constexpr uint32_t kSweepSettleMs = 250;
// A hop that never settles within this long is abandoned for the next channel and counted as a stall.
constexpr uint32_t kSweepStallMs = 3000;

class Sweep {
 public:
  // Returns false (and stays idle) when the configuration is not a usable range.
  bool start(const SweepConfig& config, uint32_t resume_hz, uint32_t now_ms);
  void stop();
  bool running() const { return running_; }
  // Call every UI tick. `receiver_ready` false pauses the schedule (nothing is counted while there is no receiver).
  // `settled` is the receiver's own settled flag for the most recently commanded channel. Returns true when the
  // application must retune to *hz now.
  bool tick(uint32_t now_ms, bool receiver_ready, bool settled, uint32_t* hz);

  uint32_t current_hz() const { return current_hz_; }
  uint32_t resume_hz() const { return resume_hz_; }
  uint32_t hops() const { return hops_; }
  uint32_t stalls() const { return stalls_; }
  uint32_t unobserved_ms() const { return unobserved_ms_; }
  const SweepConfig& config() const { return config_; }

 private:
  uint32_t next_hz();
  SweepConfig config_{};
  bool running_ = false;
  bool commanded_ = false;      // a retune was requested and has not settled yet
  bool dwelling_ = false;
  bool forward_ = true;
  uint32_t current_hz_ = 0, resume_hz_ = 0;
  uint32_t command_ms_ = 0, settled_ms_ = 0;
  uint32_t hops_ = 0, stalls_ = 0, unobserved_ms_ = 0;
};

}  // namespace orcsdr::dead_air
