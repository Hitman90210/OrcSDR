#include "p25_voice.hpp"

#include <algorithm>
#include <cstring>

namespace orcsdr::p25voice {
namespace {

constexpr uint8_t kImbeDeinterleave[p25core::kVoiceFrameBits] = {
    132,127,120,115,108,103,96,91,84,79,72,67,60,55,48,43,36,31,24,19,12,7,0,
    126,121,114,109,102,97,90,85,78,73,66,61,54,49,42,37,30,25,18,13,6,1,139,
    122,117,110,105,98,93,86,81,74,69,62,57,50,45,38,33,26,21,14,9,2,138,133,
    116,111,104,99,92,87,80,75,68,63,56,51,44,39,32,27,20,15,8,3,141,134,129,
    64,59,52,47,40,35,28,23,16,11,4,140,135,128,123,10,5,143,136,131,124,119,
    112,107,100,95,88,83,76,71,101,94,89,82,77,70,65,58,53,46,41,34,29,22,17,
    142,137,130,125,118,113,106};
constexpr uint8_t kRows[8] = {23, 23, 23, 23, 15, 15, 15, 7};

void make_matrix(const p25core::VoiceFrame& frame, char output[8][23]) {
  size_t vector = 0;
  std::memset(output, 0, 8 * 23);
  for (size_t row = 0; row < 8; ++row)
    for (size_t column = 0; column < kRows[row]; ++column)
      output[row][column] = static_cast<char>(frame.bits[kImbeDeinterleave[vector++]]);
}

int16_t safe_sample(int16_t sample) {
  constexpr int32_t kPeak = 10000;
  const int32_t scaled = static_cast<int32_t>(sample) * 3 / 5;
  return static_cast<int16_t>(std::clamp(scaled, -kPeak, kPeak));
}

}  // namespace

Decoder::Decoder() { reset(); }

void Decoder::reset() {
  previous_sample_ = 0;
  mbe_initMbeParms(&current_, &previous_, &enhanced_);
}

bool Decoder::process(const p25core::VoiceFrame& frame,
                      int16_t output[kPcmSamplesPerFrame], Result* result) {
  if (output == nullptr) return false;
  char matrix[8][23]{};
  char decoded[88]{};
  char error_text[64]{};
  int16_t pcm8k[160]{};
  int errors = 0, total_errors = 0;
  make_matrix(frame, matrix);
  mbe_processImbe7200x4400Frame(pcm8k, &errors, &total_errors, error_text,
                                matrix, decoded, &current_, &previous_,
                                &enhanced_, 1);
  size_t write = 0;
  for (const int16_t sample : pcm8k) {
    const int16_t safe = safe_sample(sample);
    const int32_t delta = static_cast<int32_t>(safe) - previous_sample_;
    for (int phase = 1; phase <= 6; ++phase)
      output[write++] = static_cast<int16_t>(previous_sample_ + delta * phase / 6);
    previous_sample_ = safe;
  }
  if (result != nullptr)
    *result = {write, static_cast<uint32_t>(std::max(0, total_errors))};
  return write == kPcmSamplesPerFrame;
}

bool Decoder::self_check() {
  constexpr uint8_t kOnAir[18] = {
      0x84,0xC6,0xA9,0x94,0x03,0xFF,0x81,0xC8,0x26,
      0x14,0x2C,0x03,0x90,0xEC,0x85,0x33,0x59,0xBC};
  constexpr uint8_t kExpected[11] = {
      0x89,0xEC,0x59,0x0E,0xB5,0x6D,0x85,0xFE,0x76,0xC4,0xC0};
  p25core::VoiceFrame frame{};
  for (size_t bit = 0; bit < p25core::kVoiceFrameBits; ++bit)
    frame.bits[bit] = (kOnAir[bit / 8] >> (7 - bit % 8)) & 1u;
  char matrix[8][23]{};
  char decoded[88]{};
  make_matrix(frame, matrix);
  (void)mbe_eccImbe7200x4400C0(matrix);
  mbe_demodulateImbe7200x4400Data(matrix);
  (void)mbe_eccImbe7200x4400Data(matrix, decoded);
  for (size_t bit = 0; bit < 88; ++bit)
    if ((decoded[bit] & 1) != ((kExpected[bit / 8] >> (7 - bit % 8)) & 1u))
      return false;
  return true;
}

}  // namespace orcsdr::p25voice
