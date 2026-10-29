#include "dead_air_sweep.hpp"

namespace orcsdr::dead_air {
namespace {
// Plain channel lists; nothing here is a claim about where anything might be heard.
constexpr SweepPreset kPresets[kSweepPresetCount] = {
    {"FM BROADCAST", {88100000u, 107900000u, 200000u, 500u, SweepPattern::forward}},
    {"AIRBAND", {118000000u, 136900000u, 100000u, 500u, SweepPattern::forward}},
};
}  // namespace

const SweepPreset& sweep_preset(uint8_t index) { return kPresets[index < kSweepPresetCount ? index : 0]; }

bool Sweep::start(const SweepConfig& config, uint32_t resume_hz, uint32_t now_ms) {
  if (config.step_hz == 0 || config.dwell_ms == 0 || config.start_hz == 0 || config.end_hz <= config.start_hz) return false;
  config_ = config;
  running_ = true;
  dwelling_ = false;
  forward_ = true;
  resume_hz_ = resume_hz;
  current_hz_ = 0;  // first tick commands start_hz
  hops_ = stalls_ = unobserved_ms_ = 0;
  commanded_ = false;
  command_ms_ = settled_ms_ = now_ms;
  return true;
}

void Sweep::stop() {
  running_ = false;
  commanded_ = dwelling_ = false;
}

uint32_t Sweep::next_hz() {
  if (current_hz_ == 0) return config_.start_hz;
  if (config_.pattern == SweepPattern::bounce) {
    if (forward_) {
      if (current_hz_ + config_.step_hz > config_.end_hz) { forward_ = false; return current_hz_ - config_.step_hz; }
      return current_hz_ + config_.step_hz;
    }
    if (current_hz_ < config_.start_hz + config_.step_hz) { forward_ = true; return current_hz_ + config_.step_hz; }
    return current_hz_ - config_.step_hz;
  }
  const uint32_t next = current_hz_ + config_.step_hz;
  return next > config_.end_hz ? config_.start_hz : next;
}

bool Sweep::tick(uint32_t now_ms, bool receiver_ready, bool settled, uint32_t* hz) {
  if (!running_ || !hz) return false;
  if (!receiver_ready) {
    // No receiver: nothing is observed and nothing is scheduled. Re-command the same channel when it returns.
    commanded_ = dwelling_ = false;
    command_ms_ = now_ms;
    if (current_hz_ == 0) return false;
    *hz = current_hz_;
    return false;
  }
  if (dwelling_) {
    if (now_ms - settled_ms_ < config_.dwell_ms) return false;
    dwelling_ = false;
  } else if (commanded_) {
    if (settled) {
      commanded_ = false;
      dwelling_ = true;
      settled_ms_ = now_ms;
      unobserved_ms_ += now_ms - command_ms_;
      return false;
    }
    if (now_ms - command_ms_ < kSweepStallMs) return false;
    ++stalls_;                       // abandon this channel; the time spent waiting is unobserved
    unobserved_ms_ += now_ms - command_ms_;
    commanded_ = false;
  }
  current_hz_ = next_hz();
  commanded_ = true;
  command_ms_ = now_ms;
  ++hops_;
  *hz = current_hz_;
  return true;
}

}  // namespace orcsdr::dead_air
