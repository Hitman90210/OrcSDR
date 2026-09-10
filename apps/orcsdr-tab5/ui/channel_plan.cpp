#include "channel_plan.hpp"

#include <cstdlib>
#include <iterator>

namespace orcsdr::channel_plan {
namespace {

// FRS/GMRS channel plan. 1-7 and 8-14 are the 462/467.5625 MHz interstitials,
// 15-22 the 462.5500 MHz main channels, and R15-R22 the 467.5500 MHz repeater
// *inputs* -- listening there hears the station uplinking rather than the
// repeater output, which is what 15-22 already carry. Receive-only: this is
// identification help, not authority to transmit (GMRS needs a licence).
constexpr uint32_t kGmrsChannelsHz[] = {
    462562500, 462587500, 462612500, 462637500, 462662500, 462687500,
    462712500, 467562500, 467587500, 467612500, 467637500, 467662500,
    467687500, 467712500, 462550000, 462575000, 462600000, 462625000,
    462650000, 462675000, 462700000, 462725000, 467550000, 467575000,
    467600000, 467625000, 467650000, 467675000, 467700000, 467725000};
constexpr const char* kGmrsChannelNames[] = {
    "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13",
    "14", "15", "16", "17", "18", "19", "20", "21", "22", "R15", "R16",
    "R17", "R18", "R19", "R20", "R21", "R22"};
static_assert(std::size(kGmrsChannelsHz) == std::size(kGmrsChannelNames));

// US VHF marine channel plan, receive side. Two arithmetic series define the
// band: channels 1-28 put ship transmit at 156.050 MHz + (n-1) x 50 kHz, and
// channels 60-88 at 156.025 MHz + (n-60) x 50 kHz. Consecutive channel
// *numbers* are 50 kHz apart; the two series interleave, so the closest two
// frequencies in the band are 25 kHz apart (channel 5A at 156.250 and 65A at
// 156.275), which is what the plan reports as its spacing. An "A" suffix is the
// US simplex use of a channel that is duplex internationally -- you receive on
// the ship frequency either way, which is what a receive-only scanner wants.
//
// This is the practical monitoring set: every US simplex/A working channel
// plus 16 (distress and calling), 13 (bridge-to-bridge), 09 (boater calling)
// and 22A (Coast Guard liaison and safety broadcasts). The duplex marine
// operator channels (24-28, 84-86), which would be received 4.6 MHz up on the
// coast side, are left out: they are near-dead in the US and would only slow a
// scan down. self_check() re-derives anchors from the formulas above so a
// mistyped digit cannot pass silently.
constexpr uint32_t kMarineChannelsHz[] = {
    156050000, 156250000, 156300000, 156350000, 156400000, 156450000,
    156500000, 156550000, 156600000, 156650000, 156700000, 156750000,
    156800000, 156850000, 156900000, 156950000, 157000000, 157050000,
    157100000, 157150000, 156175000, 156275000, 156325000, 156375000,
    156425000, 156475000, 156525000, 156575000, 156625000, 156675000,
    156725000, 156875000, 156925000, 156975000, 157025000, 157075000,
    157125000, 157175000, 157375000, 157425000};
constexpr const char* kMarineChannelNames[] = {
    "1A", "5A", "6", "7A", "8", "9", "10", "11", "12", "13", "14", "15",
    "16", "17", "18A", "19A", "20A", "21A", "22A", "23A", "63A", "65A",
    "66A", "67", "68", "69", "70", "71", "72", "73", "74", "77", "78A",
    "79A", "80A", "81A", "82A", "83A", "87", "88"};
static_assert(std::size(kMarineChannelsHz) == std::size(kMarineChannelNames));

// CB and weather channels are numbered by position, but the lockout CLI
// addresses every band the same way -- by the name the dashboard shows -- so
// each plan carries its own names rather than leaving two bands special.
constexpr const char* kCbChannelNames[] = {
    "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14",
    "15", "16", "17", "18", "19", "20", "21", "22", "23", "24", "25", "26",
    "27", "28", "29", "30", "31", "32", "33", "34", "35", "36", "37", "38",
    "39", "40"};
constexpr uint32_t kCbChannelsHz[] = {
    26965000, 26975000, 26985000, 27005000, 27015000, 27025000, 27035000,
    27055000, 27065000, 27075000, 27085000, 27105000, 27115000, 27125000,
    27135000, 27155000, 27165000, 27175000, 27185000, 27205000, 27215000,
    27225000, 27255000, 27235000, 27245000, 27265000, 27275000, 27285000,
    27295000, 27305000, 27315000, 27325000, 27335000, 27345000, 27355000,
    27365000, 27375000, 27385000, 27395000, 27405000};
static_assert(std::size(kCbChannelsHz) == std::size(kCbChannelNames));
static_assert(kCbChannelsHz[18] == kCbDefaultHz);

constexpr const char* kWxChannelNames[] = {"WX1", "WX2", "WX3", "WX4",
                                           "WX5", "WX6", "WX7"};
constexpr uint32_t kWxChannelsHz[] = {162400000, 162425000, 162450000,
                                      162475000, 162500000, 162525000,
                                      162550000};
static_assert(std::size(kWxChannelsHz) == std::size(kWxChannelNames));
static_assert(kWxChannelsHz[0] == kWxDefaultHz);

constexpr bool cb_plan_valid() {
  for (size_t i = 0; i < std::size(kCbChannelsHz); ++i) {
    if (kCbChannelsHz[i] < 26965000 || kCbChannelsHz[i] > 27405000) return false;
    for (size_t j = i + 1; j < std::size(kCbChannelsHz); ++j)
      if (kCbChannelsHz[i] == kCbChannelsHz[j]) return false;
  }
  return true;
}
static_assert(cb_plan_valid(), "CB channel plan must contain 40 unique US channels");

// A plan that outgrew the lockout mask would silently lose the ability to lock
// its tail channels, so make it a build error instead.
static_assert(std::size(kCbChannelsHz) <= kMaxChannels);
static_assert(std::size(kGmrsChannelsHz) <= kMaxChannels);
static_assert(std::size(kWxChannelsHz) <= kMaxChannels);
static_assert(std::size(kMarineChannelsHz) <= kMaxChannels);

// Channel lockout. A scanner with no way to skip a channel is a scanner you
// cannot leave running: one permanently busy channel -- a data burst, a stuck
// carrier, a repeater sitting on a continuous tone -- and the sweep parks there
// and never moves again. Locking it out is the standard answer, so each
// channelised band carries a bitmask of channels the scan steps over.
//
// The mask is indexed by position in the band's plan, not by frequency. The
// keys are on real devices; do not rename them.
struct BandLockout {
  Band band;
  const char* key;  // NVS key, <= 15 chars
  LockMask mask;
};

BandLockout g_lockouts[] = {
    {Band::cb, "lock_cb", 0},
    {Band::gmrs, "lock_gmrs", 0},
    {Band::wx, "lock_wx", 0},
    {Band::marine, "lock_marine", 0},
};

BandLockout* lockout_for(Band band) {
  for (BandLockout& entry : g_lockouts)
    if (entry.band == band) return &entry;
  return nullptr;
}

}  // namespace

Plan for_band(Band band) {
  switch (band) {
    case Band::cb:
      return {kCbChannelsHz, kCbChannelNames, std::size(kCbChannelsHz), 10000};
    // 12.5 kHz, not the 25 kHz a channel-number-to-channel-number step covers:
    // the FRS interstitials (ch 1-7) sit exactly halfway between the main GMRS
    // channels (ch 15-22), so 462.5500 and 462.5625 are the real neighbours.
    // Using 25 kHz here would have let ch 15's carrier stop the scan on ch 1.
    case Band::gmrs:
      return {kGmrsChannelsHz, kGmrsChannelNames, std::size(kGmrsChannelsHz), 12500};
    case Band::wx:
      return {kWxChannelsHz, kWxChannelNames, std::size(kWxChannelsHz), 25000};
    case Band::marine:
      return {kMarineChannelsHz, kMarineChannelNames, std::size(kMarineChannelsHz),
              25000};
    default: return {};
  }
}

bool channelized(Band band) { return for_band(band).channels_hz != nullptr; }

const char* name_for(Band band, size_t index) {
  const Plan plan = for_band(band);
  if (plan.names == nullptr || index >= plan.count) return "?";
  return plan.names[index];
}

uint32_t nearest(Band band, uint32_t frequency_hz) {
  const Plan plan = for_band(band);
  if (plan.channels_hz == nullptr || plan.count == 0) return frequency_hz;
  uint32_t best = plan.channels_hz[0];
  uint32_t best_distance = UINT32_MAX;
  for (size_t i = 0; i < plan.count; ++i) {
    const uint32_t channel = plan.channels_hz[i];
    const uint32_t distance =
        channel > frequency_hz ? channel - frequency_hz : frequency_hz - channel;
    if (distance < best_distance) {
      best_distance = distance;
      best = channel;
    }
  }
  return best;
}

size_t index_of(Band band, uint32_t frequency_hz) {
  const Plan plan = for_band(band);
  if (plan.channels_hz == nullptr) return SIZE_MAX;
  const uint32_t snapped = nearest(band, frequency_hz);
  for (size_t i = 0; i < plan.count; ++i)
    if (plan.channels_hz[i] == snapped) return i;
  return SIZE_MAX;
}

uint32_t step(Band band, uint32_t frequency_hz, int direction) {
  const Plan plan = for_band(band);
  if (plan.channels_hz == nullptr || plan.count == 0) return frequency_hz;
  size_t index = index_of(band, frequency_hz);
  // nearest() always lands on a member, so this is belt and braces.
  if (index == SIZE_MAX) index = 0;
  index = direction < 0 ? (index + plan.count - 1) % plan.count
                        : (index + 1) % plan.count;
  return plan.channels_hz[index];
}

bool locked(Band band, size_t index) {
  const BandLockout* entry = lockout_for(band);
  if (entry == nullptr || index >= kMaxChannels) return false;
  return (entry->mask >> index) & 1u;
}

size_t locked_count(Band band) {
  const BandLockout* entry = lockout_for(band);
  if (entry == nullptr) return 0;
  size_t count = 0;
  for (size_t i = 0; i < kMaxChannels; ++i)
    if ((entry->mask >> i) & 1u) ++count;
  return count;
}

bool set_locked(Band band, size_t index, bool value) {
  BandLockout* entry = lockout_for(band);
  const Plan plan = for_band(band);
  if (entry == nullptr || plan.channels_hz == nullptr) return false;
  if (index >= plan.count || index >= kMaxChannels) return false;
  const LockMask bit = LockMask{1} << index;
  entry->mask = value ? (entry->mask | bit) : (entry->mask & ~bit);
  return true;
}

LockMask mask(Band band) {
  const BandLockout* entry = lockout_for(band);
  return entry == nullptr ? 0 : entry->mask;
}

void set_mask(Band band, LockMask value) {
  BandLockout* entry = lockout_for(band);
  if (entry != nullptr) entry->mask = value;
}

const char* key_for(Band band) {
  const BandLockout* entry = lockout_for(band);
  return entry == nullptr ? nullptr : entry->key;
}

size_t lockout_band_count() { return std::size(g_lockouts); }

Band lockout_band(size_t index) {
  return index < std::size(g_lockouts) ? g_lockouts[index].band : Band::fm;
}

bool self_check() {
  constexpr Band bands[] = {Band::cb, Band::gmrs, Band::wx, Band::marine};
  for (const Band band : bands) {
    const Plan plan = for_band(band);
    if (plan.channels_hz == nullptr || plan.count == 0 || plan.spacing_hz == 0)
      return false;
    if (plan.names == nullptr) return false;
    for (size_t i = 0; i < plan.count; ++i) {
      // Every channel is its own nearest neighbour, and no two sit closer than
      // half the spacing -- which is what the scanner's peak-offset test
      // assumes when it decides a peak belongs to this channel.
      if (nearest(band, plan.channels_hz[i]) != plan.channels_hz[i]) return false;
      if (index_of(band, plan.channels_hz[i]) != i) return false;
      for (size_t j = i + 1; j < plan.count; ++j) {
        const uint32_t gap = plan.channels_hz[i] > plan.channels_hz[j]
                                 ? plan.channels_hz[i] - plan.channels_hz[j]
                                 : plan.channels_hz[j] - plan.channels_hz[i];
        if (gap <= plan.spacing_hz / 2) return false;
      }
    }
    // Stepping wraps in both directions and visits every channel exactly once.
    uint32_t walk = plan.channels_hz[0];
    for (size_t i = 0; i < plan.count; ++i) walk = step(band, walk, 1);
    if (walk != plan.channels_hz[0]) return false;
    if (step(band, plan.channels_hz[0], -1) != plan.channels_hz[plan.count - 1])
      return false;
  }

  // Marine frequencies are two arithmetic series, so re-derive them from the
  // formulas rather than trusting the table: channels 1-28 are
  // 156.050 MHz + (n-1) x 50 kHz and channels 60-88 are
  // 156.025 MHz + (n-60) x 50 kHz. A mistyped digit in a 40-entry table is
  // invisible by inspection and would silently tune the wrong channel -- this
  // check earned its keep immediately by catching a 25 kHz step here.
  const auto marine_hz = [](int number) -> uint32_t {
    return number <= 28 ? 156050000u + static_cast<uint32_t>(number - 1) * 50000u
                        : 156025000u + static_cast<uint32_t>(number - 60) * 50000u;
  };
  const Plan marine = for_band(Band::marine);
  for (size_t i = 0; i < marine.count; ++i) {
    // Names are the channel number with an optional US-simplex "A" suffix.
    const int number = atoi(marine.names[i]);
    if (number <= 0) return false;
    if (marine.channels_hz[i] != marine_hz(number)) return false;
  }

  // The defaults the dial comes up on have to be members of their own plans.
  if (index_of(Band::cb, kCbDefaultHz) == SIZE_MAX) return false;
  if (index_of(Band::gmrs, kGmrsDefaultHz) == SIZE_MAX) return false;
  if (index_of(Band::marine, kMarineDefaultHz) == SIZE_MAX) return false;
  if (index_of(Band::wx, kWxDefaultHz) == SIZE_MAX) return false;

  // Lockout round trip, restoring whatever the device already had.
  const LockMask saved = mask(Band::cb);
  set_mask(Band::cb, 0);
  const bool lock_ok = set_locked(Band::cb, 3, true) && locked(Band::cb, 3) &&
                       locked_count(Band::cb) == 1 &&
                       !set_locked(Band::cb, kMaxChannels, true) &&
                       set_locked(Band::cb, 3, false) && !locked(Band::cb, 3);
  set_mask(Band::cb, saved);
  return lock_ok;
}

}  // namespace orcsdr::channel_plan
