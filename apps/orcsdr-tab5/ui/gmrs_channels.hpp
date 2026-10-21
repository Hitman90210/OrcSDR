#pragma once

#include <cstdint>

// The FRS/GMRS channel plan, as far as the receiver needs it: which channels
// are 12.5 kHz and which are not.
//
// 47 CFR 95.1773 puts 12.5 kHz on the 467 MHz interstitials only -- channels
// 8-14, 0.5 W, handheld. Everything else in the plan runs at 20 kHz: the 462
// main channels (15-22), the 462 interstitials (1-7), and the eight 467 main
// channels, which are the repeater inputs for 15-22.
//
// Those two sets interleave at 12.5 kHz spacing across the 467 segment:
// 467.5500 is a wide repeater input, 467.5625 is narrow channel 8, 467.5750 is
// wide again. So a plain low/high window is wrong -- it narrows the seven
// repeater inputs that sit inside it, which is a bug this fork shipped on a
// branch until the static_asserts below were made exhaustive. The interstitials
// are the ones 12.5 kHz above each 467 main channel, which is what the 25 kHz
// modulus picks out.
//
// This lives in its own header so the plan is checked by a host test in
// seconds rather than only by a firmware build. See tests/gmrs_channels_tests.cpp.
namespace orcsdr::gmrs {

constexpr uint32_t kInterstitialFirstHz = 467562500;  // channel 8
constexpr uint32_t kInterstitialLastHz = 467712500;   // channel 14
constexpr uint32_t kInterstitialSpacingHz = 25000;
constexpr uint32_t kNarrowFilterHz = 12500;
// No kWideFilterHz here on purpose. 47 CFR 95.1773 allows 20 kHz on the wide
// channels, but this receiver's wide default is the 25 kHz it shares with
// weather and marine, in both this tree and the fork's own. A constant naming
// a width nothing applies would just be a claim; the wide path is whatever
// rtl_filter_default_hz() falls through to.

// The 467 MHz main channels: repeater inputs for channels 15-22, 20 kHz.
constexpr uint32_t kRepeaterInputFirstHz = 467550000;
constexpr uint32_t kRepeaterInputCount = 8;

constexpr bool channel_is_narrowband(uint32_t frequency_hz) {
  return frequency_hz >= kInterstitialFirstHz &&
         frequency_hz <= kInterstitialLastHz &&
         (frequency_hz - kInterstitialFirstHz) % kInterstitialSpacingHz == 0;
}

// Channels 8-14 are narrow, every one of them.
static_assert(channel_is_narrowband(467562500), "channel 8");
static_assert(channel_is_narrowband(467587500), "channel 9");
static_assert(channel_is_narrowband(467612500), "channel 10");
static_assert(channel_is_narrowband(467637500), "channel 11");
static_assert(channel_is_narrowband(467662500), "channel 12");
static_assert(channel_is_narrowband(467687500), "channel 13");
static_assert(channel_is_narrowband(467712500), "channel 14");

// The eight 467 main channels interleaved with them are not. Exhaustive,
// because a window without the modulus passes any subset of these that happens
// to fall outside it.
static_assert(!channel_is_narrowband(467550000), "RPT15 is a 20 kHz main");
static_assert(!channel_is_narrowband(467575000), "RPT16 is a 20 kHz main");
static_assert(!channel_is_narrowband(467600000), "RPT17 is a 20 kHz main");
static_assert(!channel_is_narrowband(467625000), "RPT18 is a 20 kHz main");
static_assert(!channel_is_narrowband(467650000), "RPT19 is a 20 kHz main");
static_assert(!channel_is_narrowband(467675000), "RPT20 is a 20 kHz main");
static_assert(!channel_is_narrowband(467700000), "RPT21 is a 20 kHz main");
static_assert(!channel_is_narrowband(467725000), "RPT22 is a 20 kHz main");

// The 462 MHz segment is wide throughout.
static_assert(!channel_is_narrowband(462562500), "channel 1, a 462 interstitial");
static_assert(!channel_is_narrowband(462712500), "channel 7, a 462 interstitial");
static_assert(!channel_is_narrowband(462550000), "channel 15, a 462 main");
static_assert(!channel_is_narrowband(462725000), "channel 22, a 462 main");

// Nothing outside the band, whatever band the dial is on. This tree has no
// RtlBand::gmrs to gate on -- the fork's own main branch does, and gates there
// -- so the frequency is tested first and these bounds are what keep a
// 12.5 kHz filter off everything that is not an interstitial.
static_assert(!channel_is_narrowband(467726000), "the band ends here");
static_assert(!channel_is_narrowband(467737500), "and this is past it");
static_assert(!channel_is_narrowband(0), "and so is nothing");

}  // namespace orcsdr::gmrs
