#pragma once

#include <cstddef>
#include <cstdint>

#include "radio_session.hpp"

// The channelised bands and everything that follows from a published channel
// list: snapping a dial to the nearest channel, stepping between channels, and
// the per-channel lockout the scanner uses to skip one.
//
// This lived in main.cpp, where the same three operations were written out once
// per band -- four nearest-channel searches inside rtl_clamp_frequency, four
// step-and-wrap blocks inside rtl_step_frequency, and four index lookups beside
// a fifth generic one. Thirteen copies of three ideas, each free to drift from
// the others. Continuous bands (FM, AM, LoRa, P25, browse) still clamp and step
// in main.cpp; only the channelised ones are described here.
namespace orcsdr::channel_plan {

using Band = orcsdr::radio::Band;

struct Plan {
  const uint32_t* channels_hz = nullptr;
  const char* const* names = nullptr;
  size_t count = 0;
  // Adjacent-channel spacing, which is what bounds how far off centre a peak
  // may sit and still count as this channel's: half a channel either way. CB's
  // 10 kHz plan tolerates 5 kHz (about one 3.75 kHz spectrum bin); GMRS's
  // 12.5 kHz plan tolerates 6.25.
  uint32_t spacing_hz = 0;
};

// The lockout mask is indexed by position in the band's plan, so it has to be
// at least as wide as the longest plan. Marine is the longest at 40; the cap is
// static_asserted against every plan in the .cpp so growing one past it is a
// build error rather than a silently untouchable tail channel.
constexpr size_t kMaxChannels = 64;
using LockMask = uint64_t;

// Default channel for each band, as the dial comes up.
constexpr uint32_t kCbDefaultHz = 27185000;      // channel 19
constexpr uint32_t kGmrsDefaultHz = 462562500;   // channel 15
constexpr uint32_t kMarineDefaultHz = 156800000; // channel 16, distress
constexpr uint32_t kWxDefaultHz = 162400000;     // WX1

Plan for_band(Band band);
bool channelized(Band band);
// The channel's printed name, which is not always its position: GMRS entry 23
// prints as "R15". "?" when the band or index has no name.
const char* name_for(Band band, size_t index);

// Snap to the nearest channel. Returns frequency_hz unchanged for a band with
// no plan, so a caller can apply this before its own continuous-band clamp.
uint32_t nearest(Band band, uint32_t frequency_hz);
// One channel up or down, wrapping at both ends. Returns frequency_hz unchanged
// for a band with no plan.
uint32_t step(Band band, uint32_t frequency_hz, int direction);
// Position of a frequency within its band's plan, or SIZE_MAX.
size_t index_of(Band band, uint32_t frequency_hz);

bool locked(Band band, size_t index);
size_t locked_count(Band band);
// False when the band has no plan or the index is out of range. Does not
// persist -- the caller owns NVS and should write mask() under key_for().
bool set_locked(Band band, size_t index, bool value);
LockMask mask(Band band);
void set_mask(Band band, LockMask value);
// NVS key for this band's mask, or nullptr. Stable: these are on real devices.
const char* key_for(Band band);
// Every band that carries a lockout mask, for load and save loops.
size_t lockout_band_count();
Band lockout_band(size_t index);

bool self_check();

}  // namespace orcsdr::channel_plan
