#pragma once

#include <cstddef>
#include <cstdint>

#include "p25_decoder_core.hpp"

extern "C" {
#include "mbelib.h"
}

namespace orcsdr::p25voice {

constexpr size_t kPcmSamplesPerFrame = 960;

struct Result {
  size_t pcm_samples = 0;
  uint32_t errors = 0;
};

// Fixed-memory Phase I IMBE decoder. It owns vocoder history and produces
// bounded 48 kHz mono PCM; task scheduling and speaker delivery stay outside.
class Decoder {
 public:
  Decoder();
  void reset();
  bool process(const p25core::VoiceFrame& frame,
               int16_t output[kPcmSamplesPerFrame], Result* result = nullptr);
  static bool self_check();

 private:
  mbe_parms current_{};
  mbe_parms previous_{};
  mbe_parms enhanced_{};
  int16_t previous_sample_ = 0;
};

}  // namespace orcsdr::p25voice
