#include "channel_scan_plan.hpp"

#include <cctype>
#include <cstdlib>
#include <iterator>

namespace orcsdr::channel_scan_plan {
namespace {

using band_profile::Id;

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

// US VHF marine channel plan, receive side. Channels 1-28 put ship transmit at
// 156.050 MHz + (n-1) x 50 kHz and channels 60-88 at 156.025 MHz + (n-60) x
// 50 kHz; the two series interleave, so the closest two channels are 25 kHz
// apart, which is the plan's spacing. This is the practical monitoring set:
// every US simplex/A working channel plus 16 (distress and calling), 13
// (bridge-to-bridge), 09 (boater calling) and 22A (Coast Guard liaison). The
// duplex marine-operator channels, received 4.6 MHz up on the coast side, are
// left out: near-dead in the US, and they would only slow a scan down.
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

// A plan that outgrew the lockout mask would silently lose the ability to lock
// its tail channels; make that a build error instead.
static_assert(std::size(kGmrsChannelsHz) <= kMaxChannels);
static_assert(std::size(kMarineChannelsHz) <= kMaxChannels);

constexpr Plan kPlans[] = {
    {Id::frs_gmrs, "GMRS", kGmrsChannelsHz, kGmrsChannelNames, std::size(kGmrsChannelsHz),
     12500, "lock_gmrs"},
    {Id::marine, "MARINE", kMarineChannelsHz, kMarineChannelNames, std::size(kMarineChannelsHz),
     25000, "lock_marine"},
};
LockMask g_masks[std::size(kPlans)] = {};

size_t slot_of(const Plan& plan) {
  for (size_t i = 0; i < std::size(kPlans); ++i)
    if (&kPlans[i] == &plan) return i;
  return SIZE_MAX;
}

bool name_equal(const char* a, const char* b) {
  if (a == nullptr || b == nullptr) return false;
  for (; *a && *b; ++a, ++b)
    if (std::toupper(static_cast<unsigned char>(*a)) !=
        std::toupper(static_cast<unsigned char>(*b))) return false;
  return *a == '\0' && *b == '\0';
}

}  // namespace

const Plan* for_band(band_profile::Id band) {
  for (const auto& plan : kPlans)
    if (plan.band == band) return &plan;
  return nullptr;
}

const Plan* for_frequency(uint32_t frequency_hz) {
  return for_band(band_profile::resolve(band_profile::Region::us, frequency_hz).id);
}

size_t index_of(const Plan& plan, uint32_t frequency_hz) {
  for (size_t i = 0; i < plan.count; ++i)
    if (plan.channels_hz[i] == frequency_hz) return i;
  return SIZE_MAX;
}

size_t nearest(const Plan& plan, uint32_t frequency_hz) {
  size_t best = SIZE_MAX;
  uint32_t best_distance = UINT32_MAX;
  for (size_t i = 0; i < plan.count; ++i) {
    const uint32_t c = plan.channels_hz[i];
    const uint32_t distance = c > frequency_hz ? c - frequency_hz : frequency_hz - c;
    if (distance < best_distance) { best_distance = distance; best = i; }
  }
  return best;
}

size_t find_name(const Plan& plan, const char* name) {
  for (size_t i = 0; i < plan.count; ++i)
    if (name_equal(plan.names[i], name)) return i;
  return SIZE_MAX;
}

LockMask mask(const Plan& plan) {
  const size_t slot = slot_of(plan);
  return slot == SIZE_MAX ? 0 : g_masks[slot];
}

void set_mask(const Plan& plan, LockMask value) {
  const size_t slot = slot_of(plan);
  if (slot == SIZE_MAX) return;
  // Bits past the end of the plan mean nothing; keep them clear so counts and
  // "everything locked" stay honest.
  const LockMask valid = plan.count >= 64 ? ~LockMask{0} : ((LockMask{1} << plan.count) - 1);
  g_masks[slot] = value & valid;
}

bool locked(const Plan& plan, size_t index) {
  return index < plan.count && ((mask(plan) >> index) & 1u) != 0;
}

bool set_locked(const Plan& plan, size_t index, bool value) {
  if (index >= plan.count) return false;
  const LockMask bit = LockMask{1} << index;
  set_mask(plan, value ? (mask(plan) | bit) : (mask(plan) & ~bit));
  return true;
}

size_t locked_count(const Plan& plan) {
  size_t n = 0;
  for (size_t i = 0; i < plan.count; ++i) n += locked(plan, i) ? 1 : 0;
  return n;
}

size_t plan_count() { return std::size(kPlans); }
const Plan& plan_at(size_t index) { return kPlans[index < std::size(kPlans) ? index : 0]; }

bool is_busy(const Plan& plan, int32_t peak_offset_hz, float snr_db, float level_dbfs,
             float min_snr_db, float squelch_dbfs) {
  if (plan.spacing_hz == 0) return false;
  const int32_t tolerance_hz = static_cast<int32_t>(plan.spacing_hz / 2);
  const int32_t distance_hz = peak_offset_hz < 0 ? -peak_offset_hz : peak_offset_hz;
  return distance_hz <= tolerance_hz && snr_db >= min_snr_db && level_dbfs >= squelch_dbfs;
}

size_t build_pass(const Plan& plan, uint32_t after_hz, uint32_t* out, size_t capacity) {
  if (out == nullptr || capacity == 0 || plan.count == 0) return 0;
  size_t first = 0;
  const size_t at = index_of(plan, after_hz);
  if (at != SIZE_MAX) first = (at + 1) % plan.count;
  size_t written = 0;
  for (size_t i = 0; i < plan.count && written < capacity; ++i) {
    const size_t index = (first + i) % plan.count;
    if (locked(plan, index)) continue;
    out[written++] = plan.channels_hz[index];
  }
  return written;
}

bool self_check() {
  for (const auto& plan : kPlans) {
    if (plan.count == 0 || plan.count > kMaxChannels || plan.lock_key == nullptr) return false;
    size_t key_length = 0;
    while (plan.lock_key[key_length]) ++key_length;
    if (key_length > 15) return false;  // NVS key limit
    // Every channel resolves to its own band, is unique, and no two sit closer
    // than the spacing the busy test assumes.
    for (size_t i = 0; i < plan.count; ++i) {
      if (for_frequency(plan.channels_hz[i]) != &plan) return false;
      for (size_t j = i + 1; j < plan.count; ++j) {
        const uint32_t a = plan.channels_hz[i], b = plan.channels_hz[j];
        if ((a > b ? a - b : b - a) < plan.spacing_hz) return false;
      }
      if (index_of(plan, plan.channels_hz[i]) != i || nearest(plan, plan.channels_hz[i]) != i)
        return false;
      if (find_name(plan, plan.names[i]) != i) return false;
    }
  }
  // Marine anchors re-derived from the two formulas, so a mistyped digit fails.
  const Plan* marine = for_band(Id::marine);
  if (marine == nullptr || marine->channels_hz[find_name(*marine, "16")] != 156050000u + 15u * 50000u ||
      marine->channels_hz[find_name(*marine, "68")] != 156025000u + 8u * 50000u) return false;
  const Plan* gmrs = for_band(Id::frs_gmrs);
  if (gmrs == nullptr || gmrs->channels_hz[find_name(*gmrs, "R15")] != 467550000u) return false;
  return true;
}

}  // namespace orcsdr::channel_scan_plan
