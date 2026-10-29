#pragma once

#include <cstddef>
#include <cstdint>

// Estimates how far the Tab5's UTC clock is from true time using the stations it decodes. Most FT8 stations keep their clocks on
// NTP or GPS, so their transmissions start within a few hundred milliseconds of the true slot boundary. If our clock is fast by e,
// our slots start e early and every decoded station shows DT = +e; if it is slow, DT = -e. The median DT of several stations
// therefore measures our clock error, without Wi-Fi and without extra hardware. Pure logic (no hardware), host-tested.
namespace orcsdr::ft8::clock_dt {

// DT a correct clock shows on this receiver (audio-chain delay and decoder timing), measured on the device after a network sync.
// The estimate is the median DT minus this. Measured 2026-10-21 on a Tab5 on 40 m with the clock within about 90 ms of true time: 43 decodes from
// 11 stations, median +60 ms, mean +84 ms, spread (median absolute deviation) 160 ms. One unit, one band and one evening: treat it as good to
// about +-100 ms, which is why the correction threshold is 200 ms.
constexpr int32_t kReceiverBiasMs = 60;

// Decodes from the last 24 slots (6 minutes) are pooled. A quiet band gives about one decode every two slots, so 8 slots (2 minutes) never
// reached the 6 decodes needed (measured on 40 m: 17 minutes of wide-search decoding without a correction). The clock drifts about 0.9 s a
// day, so a slower decision costs nothing; the evidence is cleared whenever the clock is stepped.
constexpr size_t kWindowSlots = 24;
constexpr size_t kMaxSamples = 64;
constexpr size_t kMinSamples = 6;           // fewer decodes than this prove nothing
constexpr size_t kMinDistinct = 4;          // from at least this many different stations (one chatty station must not steer the clock)
constexpr int32_t kMaxSpreadMs = 250;       // median absolute deviation across stations: honest stations agree with each other
constexpr int32_t kStepThresholdMs = 200;   // inside this the clock is good enough and is left alone
constexpr int32_t kMaxStepMs = 6000;        // a larger apparent error is not believed

struct Sample {
  int16_t dt_ms = 0;
  uint32_t station = 0;   // any stable id of the transmitting station (a hash of its callsign)
};

struct Estimate {
  bool valid = false;
  int32_t error_ms = 0;     // our clock minus true time (positive = our clock is fast)
  int32_t spread_ms = 0;
  uint16_t count = 0;
  uint16_t stations = 0;
};

class Estimator {
 public:
  void clear();
  // Adds one slot's decodes (call once per decoded slot, even with none, so old slots age out).
  void add_slot(const Sample* samples, size_t count);
  Estimate estimate() const;
  // The step to apply to the clock (added to it, in ms), or 0 when no correction is warranted.
  int32_t step_ms() const;

 private:
  struct Entry {
    int16_t dt_ms;
    uint32_t station;
    uint32_t slot;
  };
  Entry entries_[kMaxSamples]{};
  size_t count_ = 0;
  uint32_t slot_counter_ = 0;
};

// A stable id for a callsign (FNV-1a), so the estimator can tell stations apart without storing text.
uint32_t station_id(const char* callsign);

bool self_check();

}  // namespace orcsdr::ft8::clock_dt
