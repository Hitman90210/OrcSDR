#include "gmrs_channels.hpp"

#include <cstdint>
#include <cstdio>

// The whole 30-channel FRS/GMRS plan at runtime, not just the samples the
// header's static_asserts name. 23 channels are 20 kHz and 7 are 12.5 kHz, and
// the two sets interleave 12.5 kHz apart across the 467 segment, so the filter
// has to be chosen per channel rather than per band.

#define CHECK(expression)                                                      \
  do {                                                                         \
    if (!(expression)) {                                                       \
      std::printf("GMRS_CHANNELS_TEST fail=%s line=%d\n", #expression, __LINE__); \
      return 1;                                                                \
    }                                                                          \
  } while (false)

int main() {
  using namespace orcsdr::gmrs;

  // Channels 1-7: the 462 MHz interstitials, 20 kHz.
  for (int i = 0; i < 7; ++i)
    CHECK(!channel_is_narrowband(462562500u + static_cast<uint32_t>(i) * 25000u));

  // Channels 15-22: the 462 MHz main channels, 20 kHz.
  for (int i = 0; i < 8; ++i)
    CHECK(!channel_is_narrowband(462550000u + static_cast<uint32_t>(i) * 25000u));

  // Channels 8-14: the 467 MHz interstitials, 12.5 kHz. All of them.
  int narrow = 0;
  for (int i = 0; i < 7; ++i) {
    const uint32_t hz = kInterstitialFirstHz + static_cast<uint32_t>(i) * kInterstitialSpacingHz;
    CHECK(channel_is_narrowband(hz));
    ++narrow;
  }
  CHECK(narrow == 7);

  // The eight 467 MHz main channels: repeater inputs for 15-22, 20 kHz, and
  // each one sits 12.5 kHz below an interstitial. This is the case a plain
  // low/high window gets wrong.
  for (uint32_t i = 0; i < kRepeaterInputCount; ++i) {
    const uint32_t hz = kRepeaterInputFirstHz + i * kInterstitialSpacingHz;
    CHECK(!channel_is_narrowband(hz));
    const uint32_t interstitial = hz + 12500u;
    CHECK(channel_is_narrowband(interstitial) ==
          (interstitial >= kInterstitialFirstHz && interstitial <= kInterstitialLastHz));
  }

  // Every 12.5 kHz point across the 467 segment alternates wide, narrow, wide.
  for (uint32_t hz = kRepeaterInputFirstHz; hz <= 467725000u; hz += 12500u) {
    const bool is_interstitial = (hz - kInterstitialFirstHz) % 25000u == 0 &&
                                 hz >= kInterstitialFirstHz && hz <= kInterstitialLastHz;
    CHECK(channel_is_narrowband(hz) == is_interstitial);
  }

  // Outside the band nothing is narrow, because the receiver tests the
  // frequency before it looks at the band.
  CHECK(!channel_is_narrowband(0));
  CHECK(!channel_is_narrowband(kInterstitialFirstHz - 1));
  CHECK(!channel_is_narrowband(kInterstitialLastHz + 1));
  CHECK(!channel_is_narrowband(467726000));
  CHECK(!channel_is_narrowband(467737500));
  CHECK(!channel_is_narrowband(UINT32_MAX));

  CHECK(kNarrowFilterHz == 12500);

  std::printf("GMRS_CHANNELS_TEST pass\n");
  return 0;
}
