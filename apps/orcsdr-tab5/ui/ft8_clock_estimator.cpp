#include "ft8_clock_estimator.hpp"

#include <algorithm>
#include <cstdlib>

namespace orcsdr::ft8::clock_dt {
namespace {

int32_t median_of(int32_t* values, size_t n) {
  std::sort(values, values + n);
  return n % 2 == 1 ? values[n / 2] : (values[n / 2 - 1] + values[n / 2]) / 2;
}

}  // namespace

uint32_t station_id(const char* callsign) {
  uint32_t hash = 2166136261u;
  for (const char* p = callsign; p != nullptr && *p != '\0'; ++p) {
    hash ^= static_cast<uint8_t>(*p);
    hash *= 16777619u;
  }
  return hash;
}

void Estimator::clear() {
  count_ = 0;
}

void Estimator::add_slot(const Sample* samples, size_t count) {
  ++slot_counter_;
  // Age out slots older than the window.
  size_t kept = 0;
  for (size_t i = 0; i < count_; ++i)
    if (slot_counter_ - entries_[i].slot < kWindowSlots) entries_[kept++] = entries_[i];
  count_ = kept;
  for (size_t i = 0; i < count && samples != nullptr; ++i) {
    if (count_ == kMaxSamples) {   // full: drop the oldest
      for (size_t j = 1; j < count_; ++j) entries_[j - 1] = entries_[j];
      --count_;
    }
    entries_[count_++] = Entry{samples[i].dt_ms, samples[i].station, slot_counter_};
  }
}

Estimate Estimator::estimate() const {
  Estimate out{};
  out.count = static_cast<uint16_t>(count_);
  if (count_ < kMinSamples) return out;

  // One vote per station: a station that sends every slot must not outweigh the others, so each station's DTs are reduced to their
  // median first and the estimate is the median over stations.
  uint32_t ids[kMaxSamples];
  int32_t per_station[kMaxSamples];
  size_t distinct = 0;
  for (size_t i = 0; i < count_; ++i) {
    bool seen = false;
    for (size_t j = 0; j < distinct; ++j)
      if (ids[j] == entries_[i].station) {
        seen = true;
        break;
      }
    if (seen) continue;
    int32_t own[kMaxSamples];
    size_t n = 0;
    for (size_t k = i; k < count_; ++k)
      if (entries_[k].station == entries_[i].station) own[n++] = entries_[k].dt_ms;
    ids[distinct] = entries_[i].station;
    per_station[distinct] = median_of(own, n);
    ++distinct;
  }
  out.stations = static_cast<uint16_t>(distinct);
  if (distinct < kMinDistinct) return out;

  int32_t values[kMaxSamples];
  for (size_t i = 0; i < distinct; ++i) values[i] = per_station[i];
  const int32_t median = median_of(values, distinct);
  for (size_t i = 0; i < distinct; ++i) values[i] = std::abs(per_station[i] - median);
  out.spread_ms = median_of(values, distinct);
  out.error_ms = median - kReceiverBiasMs;
  out.valid = out.spread_ms <= kMaxSpreadMs;
  return out;
}

int32_t Estimator::step_ms() const {
  const Estimate e = estimate();
  if (!e.valid || std::abs(e.error_ms) < kStepThresholdMs || std::abs(e.error_ms) > kMaxStepMs) return 0;
  return -e.error_ms;
}

bool self_check() {
  Estimator est;
  Sample s[8];
  const int16_t dts[8] = {1980, 2100, 2050, 1900, 2200, 2010, 1990, 2060};
  for (size_t i = 0; i < 8; ++i) s[i] = Sample{dts[i], station_id(i == 0 ? "K1ABC" : i == 1 ? "W2DEF" : i == 2 ? "N3GHI" : i == 3 ? "VE4JKL" : i == 4 ? "K5MNO" : i == 5 ? "W6PQR" : i == 6 ? "JA7STU" : "DL8VWX")};
  est.add_slot(s, 8);
  const Estimate e = est.estimate();
  return e.valid && e.error_ms > 1900 && e.error_ms < 2100 && est.step_ms() < -1900;
}

}  // namespace orcsdr::ft8::clock_dt
