// Host test for the GMRS/marine channel-scan plans: the tables, the lockout
// mask, the order a scan pass visits channels in, and the stop test.
#include "../apps/orcsdr-tab5/ui/channel_scan_plan.hpp"

#include <cassert>
#include <cstdint>
#include <cstdio>

using namespace orcsdr::channel_scan_plan;
using orcsdr::band_profile::Id;

int main() {
  assert(self_check());

  const Plan* gmrs = for_band(Id::frs_gmrs);
  const Plan* marine = for_band(Id::marine);
  assert(gmrs && marine && gmrs->count == 30 && marine->count == 40);
  assert(for_frequency(462562500) == gmrs);   // channel 1
  assert(for_frequency(467725000) == gmrs);   // R22, the top of the band
  assert(for_frequency(156800000) == marine); // 16
  assert(for_frequency(27185000) == nullptr); // CB has upstream's own scanner
  assert(for_frequency(146520000) == nullptr);

  // Names are case-insensitive and are not positions: GMRS entry 22 is R15.
  assert(find_name(*gmrs, "r15") == 22 && gmrs->channels_hz[22] == 467550000);
  assert(find_name(*marine, "22a") == 18 && marine->channels_hz[18] == 157100000);
  assert(find_name(*gmrs, "23") == SIZE_MAX);
  assert(nearest(*gmrs, 462563000) == 0);

  // The NVS keys are the pre-consolidation line's, so lockouts survive.
  assert(gmrs->lock_key[0] && marine->lock_key[0]);

  // Lockout and pass order.
  set_mask(*gmrs, 0);
  uint32_t pass[kMaxChannels];
  assert(build_pass(*gmrs, 462562500, pass, kMaxChannels) == 30);
  assert(pass[0] == 462587500);           // starts after the dial
  assert(pass[29] == 462562500);          // and comes round to it last
  assert(set_locked(*gmrs, 1, true) && locked(*gmrs, 1) && locked_count(*gmrs) == 1);
  assert(build_pass(*gmrs, 462562500, pass, kMaxChannels) == 29);
  assert(pass[0] == 462612500);           // channel 2 skipped
  assert(!set_locked(*gmrs, 30, true));   // out of range
  set_mask(*gmrs, ~LockMask{0});
  assert(locked_count(*gmrs) == 30);      // bits past the plan are dropped
  assert(build_pass(*gmrs, 0, pass, kMaxChannels) == 0);  // everything locked
  set_mask(*gmrs, 0);
  assert(build_pass(*marine, 1, pass, kMaxChannels) == 40 && pass[0] == marine->channels_hz[0]);

  // Stop test: on channel, above the SNR floor and above squelch.
  assert(is_busy(*gmrs, 0, 25.0f, -60.0f, 20.0f, -75.0f));
  assert(is_busy(*gmrs, 6250, 25.0f, -60.0f, 20.0f, -75.0f));    // half a channel off
  assert(!is_busy(*gmrs, 6300, 25.0f, -60.0f, 20.0f, -75.0f));   // the neighbour's signal
  assert(!is_busy(*gmrs, 0, 19.9f, -60.0f, 20.0f, -75.0f));      // noise-level SNR
  assert(!is_busy(*gmrs, 0, 25.0f, -80.0f, 20.0f, -75.0f));      // under squelch
  assert(is_busy(*marine, -12500, 25.0f, -60.0f, 20.0f, -75.0f)); // marine spacing is 25 kHz

  std::puts("channel_scan_plan_tests: PASS");
  return 0;
}
