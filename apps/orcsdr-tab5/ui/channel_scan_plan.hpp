#pragma once

#include <cstddef>
#include <cstdint>

#include "band_profile.hpp"

// Channel lists for the scanner on bands whose channels are not simply "every
// point on the raster": FRS/GMRS (two segments 5 MHz apart) and US VHF marine
// (a monitoring subset of a 25 kHz grid). band_profile says how to tune there;
// this says which frequencies are channels, what they are called, and which
// ones the user has locked out of scans.
//
// Ported from the pre-consolidation fork's channel_plan, which keyed the same
// tables on RtlBand::gmrs / RtlBand::marine. This tree has no such bands --
// both are band_profile entries reached through the generic receiver -- so the
// tables are keyed on band_profile::Id instead. The NVS keys are the old ones,
// so a device upgraded from that line keeps its lockouts. Pure data and logic,
// no display or RTOS: host-tested by tests/channel_scan_plan_tests.cpp.
namespace orcsdr::channel_scan_plan {

constexpr size_t kMaxChannels = 64;
using LockMask = uint64_t;

struct Plan {
  band_profile::Id band = band_profile::Id::count;
  const char* label = "";          // "GMRS", "MARINE"
  const uint32_t* channels_hz = nullptr;
  const char* const* names = nullptr;
  size_t count = 0;
  // Adjacent-channel spacing: half of it is how far off the channel centre the
  // strongest bin may sit and still count as this channel's signal.
  uint32_t spacing_hz = 0;
  const char* lock_key = nullptr;  // NVS key for the lockout mask, <= 15 chars
};

// The plan for a band, or nullptr when the band has no scanner.
const Plan* for_band(band_profile::Id band);
// The plan whose band contains frequency_hz (via band_profile), or nullptr.
const Plan* for_frequency(uint32_t frequency_hz);

// Position of an exact channel frequency in the plan, or SIZE_MAX.
size_t index_of(const Plan& plan, uint32_t frequency_hz);
// Nearest channel to frequency_hz; SIZE_MAX for an empty plan.
size_t nearest(const Plan& plan, uint32_t frequency_hz);
// Position of the channel printed as `name` (case-insensitive), or SIZE_MAX.
size_t find_name(const Plan& plan, const char* name);

// Lockout: a channel the scan steps over. Indexed by position in the plan.
LockMask mask(const Plan& plan);
void set_mask(const Plan& plan, LockMask value);
bool locked(const Plan& plan, size_t index);
bool set_locked(const Plan& plan, size_t index, bool value);  // false if out of range
size_t locked_count(const Plan& plan);

// Every plan, for load and save loops.
size_t plan_count();
const Plan& plan_at(size_t index);

// The scan's stop test, separated out so it can be host-tested: a channel is
// busy when the strongest visible bin sits within half a channel of the dial,
// its in-window SNR clears min_snr_db, and the channel level clears squelch.
bool is_busy(const Plan& plan, int32_t peak_offset_hz, float snr_db, float level_dbfs,
             float min_snr_db, float squelch_dbfs);

// The channels a scan pass visits, in order: every unlocked channel, starting
// with the one after `after_hz` (or the first channel when after_hz is not a
// channel). Returns how many were written; 0 means everything is locked out.
size_t build_pass(const Plan& plan, uint32_t after_hz, uint32_t* out, size_t capacity);

bool self_check();

}  // namespace orcsdr::channel_scan_plan
