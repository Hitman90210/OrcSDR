#include "dead_air_model.hpp"
#include <algorithm>
#include <cmath>

namespace orcsdr::dead_air {

Status reading_status(const Reading& reading, uint32_t stale_after_ms) {
  if (!reading.present) return Status::unavailable;
  if (!reading.valid) return Status::warming_up;
  if (reading.age_ms > stale_after_ms || !std::isfinite(reading.value))
    return Status::stale;
  return Status::current;
}
float rf_rise_threshold_db(Sensitivity value) {
  switch (value) {
    case Sensitivity::quiet: return 12.0f;
    case Sensitivity::sensitive: return 5.0f;
    default: return 8.0f;
  }
}
const char* source_name(Source source) {
  switch (source) {
    case Source::rf: return "RF";
    case Source::audio: return "AUDIO";
    case Source::environment: return "ENV";
    case Source::manual: return "MANUAL";
    case Source::sweep: return "GENERATED";
  }
  return "UNKNOWN";
}
const char* kind_name(Kind kind) {
  switch (kind) {
    case Kind::rf_rise: return "RF LEVEL RISE";
    case Kind::audio_burst: return "AUDIO ACTIVITY";
    case Kind::temperature_change: return "TEMPERATURE CHANGE";
    case Kind::humidity_change: return "HUMIDITY CHANGE";
    case Kind::pressure_change: return "PRESSURE CHANGE";
    case Kind::manual_marker: return "INVESTIGATOR MARK";
    case Kind::sweep_start: return "SWEEP START";
    case Kind::sweep_stop: return "SWEEP STOP";
  }
  return "OBSERVATION";
}
void EventModel::set_listener(EventListener listener, void* context) {
  listener_ = listener;
  listener_context_ = context;
}
void EventModel::reset() {
  count_ = next_ = total_ = 0;
  config_revision_ = tuned_hz_ = candidate_since_ = cooldown_until_ = 0;
  last_valid_ms_ = 0;
  have_valid_sample_ = false;
  baseline_count_ = 0; baseline_ = 0;
  have_tune_ = pending_ = latched_ = false;
  sensitivity_ = Sensitivity::standard;
}
void EventModel::set_sensitivity(Sensitivity sensitivity) {
  if (sensitivity_ == sensitivity) return;
  sensitivity_ = sensitivity;
  // Changing the rule must not manufacture an event from a pending crossing.
  pending_ = latched_ = false;
}
void EventModel::reset_detector(uint32_t revision, uint32_t frequency) {
  config_revision_ = revision; tuned_hz_ = frequency;
  baseline_count_ = 0; baseline_ = 0;
  pending_ = latched_ = false;
  last_valid_ms_ = 0;
  have_valid_sample_ = false;
  have_tune_ = true;
}
void EventModel::push(Event event) {
  event.sequence = total_ + 1;
  events_[next_] = event;
  next_ = (next_ + 1) % kEventCapacity;
  count_ = std::min(count_ + 1, kEventCapacity);
  ++total_;
  if (listener_) listener_(listener_context_, event);
}
bool EventModel::append_measured(const Event& event) {
  const bool valid_kind =
      (event.source == Source::rf && event.kind == Kind::rf_rise &&
       event.frequency_hz != 0) ||
      (event.source == Source::audio && event.kind == Kind::audio_burst) ||
      (event.source == Source::environment &&
       (event.kind == Kind::temperature_change ||
        event.kind == Kind::humidity_change ||
        event.kind == Kind::pressure_change));
  if (!valid_kind || event.threshold <= 0.0f ||
      !std::isfinite(event.value) || !std::isfinite(event.reference) ||
      !std::isfinite(event.threshold)) return false;
  Event copy = event;
  copy.sensitivity = sensitivity_;
  push(copy);
  return true;
}
void EventModel::mark_manual(uint32_t ms, uint32_t unix_time, uint32_t tuned_hz) {
  Event entry{};
  entry.monotonic_ms = ms;
  entry.unix_time = unix_time;
  entry.frequency_hz = tuned_hz;
  entry.source = Source::manual;
  entry.kind = Kind::manual_marker;
  entry.sensitivity = sensitivity_;
  push(entry);
}
bool EventModel::mark_sweep(const Event& event) {
  if (event.source != Source::sweep || (event.kind != Kind::sweep_start && event.kind != Kind::sweep_stop)) return false;
  Event copy = event;
  copy.sensitivity = sensitivity_;
  push(copy);
  return true;
}
const Event* EventModel::newest(size_t index) const {
  if (index >= count_) return nullptr;
  return &events_[(next_ + kEventCapacity - 1 - index) % kEventCapacity];
}
void EventModel::observe_rf(const RfObservation& s) {
  // Check tuning/retune before considering a sample. A scan is discontinuous.
  if (!have_tune_ || tuned_hz_ != s.frequency_hz ||
      config_revision_ != s.configuration_revision) {
    reset_detector(s.configuration_revision, s.frequency_hz);
  }
  if (!s.valid || !s.settled || !std::isfinite(s.level_dbfs)) {
    pending_ = latched_ = false;
    baseline_count_ = 0;
    baseline_ = 0.0f;
    have_valid_sample_ = false;
    return;
  }
  // Missing or out-of-order samples cannot satisfy persistence. Treat a long
  // gap as a new observation window even without a retune.
  if (have_valid_sample_) {
    const uint32_t elapsed = s.monotonic_ms - last_valid_ms_;
    if (elapsed > 500u || static_cast<int32_t>(elapsed) <= 0) {
      reset_detector(s.configuration_revision, s.frequency_hz);
    }
  }
  last_valid_ms_ = s.monotonic_ms;
  have_valid_sample_ = true;
  if (!baseline_ready()) {
    // First samples after every retune establish a new, frequency-local floor.
    baseline_ += s.level_dbfs;
    ++baseline_count_;
    if (baseline_ready()) baseline_ /= static_cast<float>(kWarmupCount);
    return;
  }
  const float threshold = rf_rise_threshold_db(sensitivity_);
  const float excess = s.level_dbfs - baseline_;
  if (excess <= threshold * 0.5f) {
    pending_ = latched_ = false;
    // Slow adaptation only when there is no candidate excursion.
    baseline_ = baseline_ * 0.98f + s.level_dbfs * 0.02f;
    return;
  }
  if (latched_ || excess < threshold) {
    if (!latched_) pending_ = false;
    return;
  }
  if (!pending_) {
    candidate_since_ = s.monotonic_ms;
    pending_ = true;
    return;
  }
  // Unsigned subtraction intentionally handles 32-bit uptime rollover.
  if (static_cast<uint32_t>(s.monotonic_ms - candidate_since_) < 200 ||
      static_cast<int32_t>(s.monotonic_ms - cooldown_until_) < 0) return;
  Event event{};
  event.monotonic_ms = s.monotonic_ms;
  event.unix_time = s.unix_time;
  event.frequency_hz = s.frequency_hz;
  event.source = Source::rf;
  event.kind = Kind::rf_rise;
  event.value = s.level_dbfs;
  event.reference = baseline_;
  event.threshold = threshold;
  event.sensitivity = sensitivity_;
  push(event);
  pending_ = false;
  latched_ = true;
  cooldown_until_ = s.monotonic_ms + 3000;
}
bool EventModel::self_check() const {
  return count_ <= kEventCapacity && next_ < kEventCapacity &&
         count_ <= total_ && static_cast<unsigned>(sensitivity_) <= 2;
}

}  // namespace orcsdr::dead_air
