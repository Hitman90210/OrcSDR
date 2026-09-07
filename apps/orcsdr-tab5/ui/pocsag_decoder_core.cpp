#include "pocsag_decoder_core.hpp"

#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>

namespace orcsdr::pocsag {
namespace {

constexpr float kPi = 3.14159265358979323846f;

int bit_count(uint32_t value) {
  int count = 0;
  while (value != 0) {
    value &= value - 1;
    ++count;
  }
  return count;
}

uint8_t reverse_bits(uint8_t value, int width) {
  uint8_t result = 0;
  for (int i = 0; i < width; ++i) {
    result = static_cast<uint8_t>((result << 1) | (value & 1u));
    value = static_cast<uint8_t>(value >> 1);
  }
  return result;
}

// BCH(31,21) generator g(x) = x^10+x^9+x^8+x^6+x^5+x^3+1. This constant holds
// only the x^9..x^0 coefficients (0x369); the implicit x^10 leading term is
// realized by the shift-register structure below, the standard construction
// for polynomial-division-as-CRC (same technique already used for the P25
// NID/TSBK checks elsewhere in this codebase, applied to POCSAG's public
// generator polynomial rather than P25's).
constexpr uint16_t kBchPoly = 0x369u;

// Runs the 31-bit value (bit30 downto bit0, MSB first) through the BCH(31,21)
// polynomial-division shift register. For an already-encoded 31-bit
// codeword this returns the syndrome (0 == no detected error); for a 21-bit
// message left-shifted by 10 (parity bits zeroed) this returns the parity to
// place in those 10 low bits.
uint16_t bch_remainder(uint32_t value31) {
  uint32_t reg = 0;
  for (int i = 30; i >= 0; --i) {
    const uint32_t bit_in = (value31 >> i) & 1u;
    reg = (reg << 1) | bit_in;
    // reg momentarily carries an x^10 (bit10) term when a 1 bit shifts in
    // at that position; XOR by the generator (bit10 implicit + kBchPoly's
    // lower 10 bits) cancels it back into a <10-degree remainder.
    if (reg & 0x400u) reg ^= (0x400u | kBchPoly);
  }
  return static_cast<uint16_t>(reg & 0x3FFu);
}

uint32_t bch_encode(uint32_t data21) {
  const uint32_t shifted = (data21 & 0x1FFFFFu) << 10;
  return shifted | bch_remainder(shifted);
}

// Bounded (no heap) syndrome -> correction-pattern table, built once. Only
// weight-1 and weight-2 error patterns are populated: BCH(31,21) has minimum
// distance 5, so those are exactly the uniquely-correctable patterns: any
// other nonzero syndrome is a detected-but-uncorrectable (3+ bit) error.
struct CorrectionTable {
  std::array<int32_t, 1024> pattern{};
  CorrectionTable() {
    pattern.fill(-1);
    pattern[0] = 0;
    for (int i = 0; i < 31; ++i) {
      const uint32_t e = 1u << i;
      const uint16_t syndrome = bch_remainder(e);
      if (pattern[syndrome] < 0) pattern[syndrome] = static_cast<int32_t>(e);
    }
    for (int i = 0; i < 31; ++i) {
      for (int j = i + 1; j < 31; ++j) {
        const uint32_t e = (1u << i) | (1u << j);
        const uint16_t syndrome = bch_remainder(e);
        if (pattern[syndrome] < 0) pattern[syndrome] = static_cast<int32_t>(e);
      }
    }
  }
};

const CorrectionTable& correction_table() {
  static const CorrectionTable table;
  return table;
}

// status: 0 = no error, 1 = corrected, 2 = uncorrectable (3+ bit errors).
int decode_bch31(uint32_t received31, uint32_t* corrected31, int* error_bits) {
  const uint16_t syndrome = bch_remainder(received31);
  if (syndrome == 0) {
    *corrected31 = received31;
    *error_bits = 0;
    return 0;
  }
  const int32_t pattern = correction_table().pattern[syndrome];
  if (pattern < 0) {
    *corrected31 = received31;
    *error_bits = -1;
    return 2;
  }
  *corrected31 = received31 ^ static_cast<uint32_t>(pattern);
  *error_bits = bit_count(static_cast<uint32_t>(pattern));
  return 1;
}

// Builds a full 32-bit POCSAG codeword (flag + 20-bit data + 10-bit BCH
// parity + 1 even-parity bit) from a 21-bit info word (flag<<20 | data20).
// Used by encoders (host tests / a future TX-side test-vector generator);
// the live decode path only ever calls decode_bch31/bch_remainder.
uint32_t pocsag_encode_codeword(uint32_t info21) {
  const uint32_t bch31 = bch_encode(info21);
  const int parity = bit_count(bch31) & 1;
  return (bch31 << 1) | static_cast<uint32_t>(parity);
}

// Standard public POCSAG numeric 4-bit character set. Each transmitted
// nibble is bit-reversed relative to natural MSB-first order; callers pass
// the already-reversed nibble.
constexpr char kNumericTable[16] = {'0', '1', '2', '3', '4', '5', '6', '7',
                                     '8', '9', '*', 'U', '-', ')', '(', ' '};

uint8_t numeric_char_to_nibble(char c) {
  for (uint8_t i = 0; i < 16; ++i)
    if (kNumericTable[i] == c) return i;
  return 0xFu;  // '(' collides with none normally; unmapped input -> 0xF (' ')
}

}  // namespace

Decoder::Decoder() { reset(); }

void Decoder::reset() {
  stats_ = Stats{};
  active_channel_ = -1;
  codeword_index_in_batch_ = 0;
  codeword_accum_ = 0;
  codeword_bit_count_ = 0;
  message_active_ = false;
  message_capcode_ = 0;
  message_function_ = 0;
  message_type_ = MessageType::unknown;
  message_codeword_count_ = 0;
  message_corrected_bits_ = 0;
  message_uncorrectable_words_ = 0;
  message_truncated_ = false;
  message_text_length_ = 0;
  alpha_bit_accum_ = 0;
  alpha_bit_count_ = 0;
  cic_stage1_ = CicStage{};
  cic_stage2_ = CicStage{};
  dc_prev_i_ = dc_prev_q_ = dc_prev_y_i_ = dc_prev_y_q_ = 0.0f;
  discriminator_prev_i_ = 1.0f;
  discriminator_prev_q_ = 0.0f;
  discriminator_primed_ = false;
  mark_mean_ = 0.5f;
  space_mean_ = -0.5f;

  struct Spec {
    uint16_t sps;
    bool inverted;
    Baud baud;
    Polarity polarity;
  };
  static constexpr Spec kSpecs[Decoder::kChannelCount] = {
      {75, false, Baud::b512, Polarity::normal},
      {75, true, Baud::b512, Polarity::inverted},
      {32, false, Baud::b1200, Polarity::normal},
      {32, true, Baud::b1200, Polarity::inverted},
      {16, false, Baud::b2400, Polarity::normal},
      {16, true, Baud::b2400, Polarity::inverted},
  };
  for (size_t i = 0; i < kChannelCount; ++i) {
    channels_[i] = Channel{};
    const bool baud_ok =
        configured_baud_ == Baud::auto_detect || configured_baud_ == kSpecs[i].baud;
    const bool polarity_ok = configured_polarity_ == Polarity::auto_detect ||
                              configured_polarity_ == kSpecs[i].polarity;
    channels_[i].samples_per_symbol = (baud_ok && polarity_ok) ? kSpecs[i].sps : 0;
    channels_[i].inverted = kSpecs[i].inverted;
  }
}

void Decoder::configure(Baud baud, Polarity polarity) {
  configured_baud_ = baud;
  configured_polarity_ = polarity;
  reset();
}

void Decoder::record_soft_sample(float sample) {
  stats_.soft_symbols[stats_.soft_symbol_write] = sample;
  stats_.soft_symbol_write = (stats_.soft_symbol_write + 1) % Stats::kSoftSymbolCapacity;
  if (stats_.soft_symbol_count < Stats::kSoftSymbolCapacity) ++stats_.soft_symbol_count;
}

void Decoder::record_fec_outcome(uint8_t outcome) {
  stats_.fec_history[stats_.fec_history_write] = outcome;
  stats_.fec_history_write = (stats_.fec_history_write + 1) % Stats::kFecHistoryCapacity;
  if (stats_.fec_history_count < Stats::kFecHistoryCapacity) ++stats_.fec_history_count;
}

void Decoder::append_char(char c) {
  if (message_text_length_ + 1u >= kMaxMessageChars) {
    message_truncated_ = true;
    return;
  }
  message_text_[message_text_length_++] = c;
}

void Decoder::append_numeric_payload(uint32_t data20) {
  for (int i = 0; i < 5; ++i) {
    const uint8_t nibble = static_cast<uint8_t>((data20 >> (16 - 4 * i)) & 0xFu);
    const uint8_t reversed = reverse_bits(nibble, 4);
    append_char(kNumericTable[reversed]);
  }
}

void Decoder::append_alpha_payload(uint32_t data20) {
  uint32_t combined = (alpha_bit_accum_ << 20) | (data20 & 0xFFFFFu);
  int total_bits = static_cast<int>(alpha_bit_count_) + 20;
  while (total_bits >= 7) {
    total_bits -= 7;
    const uint8_t chunk = static_cast<uint8_t>((combined >> total_bits) & 0x7Fu);
    const uint8_t ascii = reverse_bits(chunk, 7);
    append_char(static_cast<char>(ascii));
  }
  alpha_bit_accum_ = combined & ((1u << total_bits) - 1u);
  alpha_bit_count_ = static_cast<uint8_t>(total_bits);
}

void Decoder::finalize_message(MessageCallback callback, void* ctx) {
  Message msg{};
  msg.capcode = message_capcode_;
  msg.function = message_function_;
  msg.type = message_codeword_count_ == 0 ? MessageType::tone_only : message_type_;
  msg.baud = active_channel_ >= 0
                 ? static_cast<uint16_t>(kInternalSampleRateHz /
                                          channels_[active_channel_].samples_per_symbol)
                 : 0;
  msg.inverted = active_channel_ >= 0 && channels_[active_channel_].inverted;
  msg.truncated = message_truncated_;
  msg.corrected_bits = message_corrected_bits_;
  msg.uncorrectable_words = message_uncorrectable_words_;
  msg.text_length = message_text_length_;
  std::memcpy(msg.text, message_text_, message_text_length_);
  msg.text[message_text_length_] = '\0';

  ++stats_.messages_decoded;
  if (msg.truncated) ++stats_.messages_truncated;
  if (callback) callback(msg, ctx);

  message_active_ = false;
  message_text_length_ = 0;
  alpha_bit_accum_ = 0;
  alpha_bit_count_ = 0;
}

void Decoder::on_codeword(uint32_t codeword32, MessageCallback callback, void* ctx) {
  ++stats_.codewords_total;
  const uint32_t bch31 = codeword32 >> 1;
  uint32_t corrected31 = 0;
  int error_bits = 0;
  int status = decode_bch31(bch31, &corrected31, &error_bits);

  if (status != 2) {
    const int expected_parity = bit_count(corrected31) & 1;
    const int received_parity = static_cast<int>(codeword32 & 1u);
    if (expected_parity != received_parity) {
      status = 2;
      ++stats_.parity_failures;
    }
  }

  if (status == 0) {
    ++stats_.codewords_valid;
    record_fec_outcome(0);
  } else if (status == 1) {
    ++stats_.codewords_corrected;
    stats_.corrected_bit_count += static_cast<uint32_t>(error_bits);
    record_fec_outcome(1);
  } else {
    ++stats_.codewords_uncorrectable;
    record_fec_outcome(2);
  }

  if (status == 2) {
    if (message_active_) {
      ++message_uncorrectable_words_;
      ++message_codeword_count_;
    }
    return;
  }

  const bool flag = ((corrected31 >> 30) & 1u) != 0;
  const uint32_t data20 = (corrected31 >> 10) & 0xFFFFFu;

  if (!flag) {
    if (corrected31 == (kIdleWord >> 1)) {
      if (message_active_) finalize_message(callback, ctx);
      return;
    }
    if (message_active_) finalize_message(callback, ctx);
    const uint32_t address18 = (data20 >> 2) & 0x3FFFFu;
    const uint8_t function = static_cast<uint8_t>(data20 & 0x3u);
    const uint8_t frame_number = static_cast<uint8_t>((codeword_index_in_batch_ / 2) & 0x7u);
    message_active_ = true;
    message_capcode_ = (address18 << 3) | frame_number;
    message_function_ = function;
    message_type_ = function == 3 ? MessageType::alpha : MessageType::numeric;
    message_codeword_count_ = 0;
    message_corrected_bits_ = static_cast<uint16_t>(status == 1 ? error_bits : 0);
    message_uncorrectable_words_ = 0;
    message_truncated_ = false;
    message_text_length_ = 0;
    alpha_bit_accum_ = 0;
    alpha_bit_count_ = 0;
  } else {
    if (!message_active_) return;
    ++message_codeword_count_;
    if (status == 1)
      message_corrected_bits_ =
          static_cast<uint16_t>(message_corrected_bits_ + error_bits);
    if (message_type_ == MessageType::alpha)
      append_alpha_payload(data20);
    else
      append_numeric_payload(data20);
    if (message_text_length_ >= kMaxMessageChars - 1) message_truncated_ = true;
  }
}

bool Decoder::search_channels(float sample, size_t* winner) {
  for (size_t i = 0; i < kChannelCount; ++i) {
    Channel& ch = channels_[i];
    if (ch.samples_per_symbol == 0) continue;
    const float signed_sample = ch.inverted ? -sample : sample;
    ch.symbol_accum += signed_sample;
    ++ch.symbol_accum_count;
    if (++ch.sample_counter < ch.samples_per_symbol) continue;
    ch.sample_counter = 0;
    const float soft =
        ch.symbol_accum_count > 0 ? ch.symbol_accum / static_cast<float>(ch.symbol_accum_count) : 0.0f;
    ch.symbol_accum = 0.0f;
    ch.symbol_accum_count = 0;
    const bool bit = soft >= 0.0f;
    if (ch.has_last_bit) {
      // Confidence decays rather than resets on a single non-alternating
      // pair: the 32-bit sync word itself is not a pure 0101... pattern
      // (e.g. 0x7CD215D8 has runs of up to 5 identical bits), so a hard
      // reset here would zero the gate out mid-word, before the 32-bit
      // correlation below ever gets a chance to fire.
      if (bit != ch.last_bit) {
        if (ch.transition_run < 255) ++ch.transition_run;
      } else if (ch.transition_run > 0) {
        --ch.transition_run;
      }
    }
    ch.last_bit = bit;
    ch.has_last_bit = true;
    ch.bit_shift = (ch.bit_shift << 1) | (bit ? 1u : 0u);
    // Require some preamble alternation before trusting a sync-word
    // correlation, to keep random noise from producing a spurious lock
    // (this repo's own false-positive-control discipline, see phasing.md).
    if (ch.transition_run >= 6) {
      const uint32_t diff = ch.bit_shift ^ kSyncWord;
      if (bit_count(diff) <= 2) {
        *winner = i;
        return true;
      }
    }
  }
  return false;
}

bool Decoder::slice_active_channel(float sample, bool* bit) {
  Channel& ch = channels_[static_cast<size_t>(active_channel_)];
  const float signed_sample = ch.inverted ? -sample : sample;
  ch.symbol_accum += signed_sample;
  ++ch.symbol_accum_count;
  if (++ch.sample_counter < ch.samples_per_symbol) return false;
  ch.sample_counter = 0;
  const float soft =
      ch.symbol_accum_count > 0 ? ch.symbol_accum / static_cast<float>(ch.symbol_accum_count) : 0.0f;
  ch.symbol_accum = 0.0f;
  ch.symbol_accum_count = 0;
  *bit = soft >= 0.0f;
  // Track mark/space discriminator-cluster means for the FSK deviation
  // estimate exposed on the SIGNAL dashboard.
  constexpr float kTrackGain = 0.02f;
  if (*bit)
    mark_mean_ += kTrackGain * (soft - mark_mean_);
  else
    space_mean_ += kTrackGain * (soft - space_mean_);
  stats_.fsk_deviation_hz =
      (mark_mean_ - space_mean_) * 0.5f * (static_cast<float>(kInternalSampleRateHz) / kPi);
  return true;
}

void Decoder::process_discriminator_sample(float sample, MessageCallback callback,
                                            void* context) {
  record_soft_sample(sample);

  if (active_channel_ < 0) {
    size_t winner = 0;
    if (search_channels(sample, &winner)) {
      active_channel_ = static_cast<int>(winner);
      Channel& ch = channels_[winner];
      ch.bit_shift = 0;
      ch.sample_counter = 0;
      ch.symbol_accum = 0.0f;
      ch.symbol_accum_count = 0;
      ch.transition_run = 0;
      ch.has_last_bit = false;
      codeword_index_in_batch_ = 0;
      codeword_accum_ = 0;
      codeword_bit_count_ = 0;
      stats_.lock = LockState::locked;
      stats_.detected_baud = static_cast<uint16_t>(kInternalSampleRateHz / ch.samples_per_symbol);
      stats_.inverted = ch.inverted;
      ++stats_.batches_synced;
    } else {
      stats_.lock = LockState::searching;
    }
    return;
  }

  bool bit = false;
  if (!slice_active_channel(sample, &bit)) return;
  codeword_accum_ = (codeword_accum_ << 1) | (bit ? 1u : 0u);
  if (++codeword_bit_count_ < 32) return;
  codeword_bit_count_ = 0;
  const uint32_t codeword = codeword_accum_;
  codeword_accum_ = 0;

  if (codeword_index_in_batch_ >= kBatchCodewords) {
    if (bit_count(codeword ^ kSyncWord) <= 2) {
      ++stats_.batches_synced;
      codeword_index_in_batch_ = 0;
    } else {
      ++stats_.sync_losses;
      stats_.lock = LockState::lost;
      if (message_active_) finalize_message(callback, context);
      active_channel_ = -1;
    }
    return;
  }

  on_codeword(codeword, callback, context);
  ++codeword_index_in_batch_;
}

void Decoder::process_cu8(const uint8_t* iq, size_t bytes, MessageCallback callback,
                          void* context) {
  if (!iq) return;
  bytes &= ~static_cast<size_t>(1);
  for (size_t i = 0; i < bytes; i += 2) {
    const float fi = (static_cast<int>(iq[i]) - 127) / 128.0f;
    const float fq = (static_cast<int>(iq[i + 1]) - 127) / 128.0f;

    // One-pole DC blocker: y[n] = x[n] - x[n-1] + R*y[n-1].
    constexpr float kDcR = 0.995f;
    const float y_i = fi - dc_prev_i_ + kDcR * dc_prev_y_i_;
    const float y_q = fq - dc_prev_q_ + kDcR * dc_prev_y_q_;
    dc_prev_i_ = fi;
    dc_prev_q_ = fq;
    dc_prev_y_i_ = y_i;
    dc_prev_y_q_ = y_q;

    // Two-stage decimate-by-5 CIC (960 kS/s -> 192 kS/s -> 38.4 kS/s). Each
    // stage is a running integrator differenced every 5 inputs, i.e. a
    // boxcar average -- the simplest anti-aliased decimator, adequate for
    // POCSAG's narrow channel; a sharper multi-tap FIR is a measured-later
    // refinement, not required to prove the chain end-to-end.
    cic_stage1_.integrator_i += y_i;
    cic_stage1_.integrator_q += y_q;
    if (++cic_stage1_.phase < 5) continue;
    cic_stage1_.phase = 0;
    const double s1_i = (cic_stage1_.integrator_i - cic_stage1_.previous_i) / 5.0;
    const double s1_q = (cic_stage1_.integrator_q - cic_stage1_.previous_q) / 5.0;
    cic_stage1_.previous_i = cic_stage1_.integrator_i;
    cic_stage1_.previous_q = cic_stage1_.integrator_q;

    cic_stage2_.integrator_i += s1_i;
    cic_stage2_.integrator_q += s1_q;
    if (++cic_stage2_.phase < 5) continue;
    cic_stage2_.phase = 0;
    const float s2_i = static_cast<float>((cic_stage2_.integrator_i - cic_stage2_.previous_i) / 5.0);
    const float s2_q = static_cast<float>((cic_stage2_.integrator_q - cic_stage2_.previous_q) / 5.0);
    cic_stage2_.previous_i = cic_stage2_.integrator_i;
    cic_stage2_.previous_q = cic_stage2_.integrator_q;

    if (!discriminator_primed_) {
      discriminator_prev_i_ = s2_i;
      discriminator_prev_q_ = s2_q;
      discriminator_primed_ = true;
      continue;
    }
    const float cross = discriminator_prev_i_ * s2_q - discriminator_prev_q_ * s2_i;
    const float dot = discriminator_prev_i_ * s2_i + discriminator_prev_q_ * s2_q;
    discriminator_prev_i_ = s2_i;
    discriminator_prev_q_ = s2_q;
    const float sample = atan2f(cross, dot) / kPi;
    process_discriminator_sample(sample, callback, context);
  }
}

namespace {

// Encodes a synthetic POCSAG batch (sync word + codewords) as a discriminator
// sample stream at the given baud, feeding self_check() below without any
// third-party decoder source.
struct TestVectorBuilder {
  float* out;
  size_t capacity;
  size_t count = 0;
  uint16_t samples_per_symbol;
  bool inverted;

  void push_bit(bool bit) {
    const float level = (bit ? 1.0f : -1.0f) * (inverted ? -1.0f : 1.0f) * 0.8f;
    for (uint16_t i = 0; i < samples_per_symbol && count < capacity; ++i) out[count++] = level;
  }
  void push_word(uint32_t word) {
    for (int i = 31; i >= 0; --i) push_bit(((word >> i) & 1u) != 0);
  }
  void push_preamble(int bits) {
    bool bit = true;
    for (int i = 0; i < bits; ++i) {
      push_bit(bit);
      bit = !bit;
    }
  }
};

}  // namespace

bool Decoder::self_check() {
  // BCH(31,21) round-trip: 0/1/2-bit errors correct, 3-bit is rejected.
  {
    const uint32_t info = (0u << 20) | (0x1234u << 2) | 0x1u;  // address, function 1
    const uint32_t bch31 = bch_encode(info);
    uint32_t corrected = 0;
    int errors = 0;
    if (decode_bch31(bch31, &corrected, &errors) != 0 || corrected != bch31) return false;
    if (decode_bch31(bch31 ^ 0x1u, &corrected, &errors) != 1 || corrected != bch31 || errors != 1)
      return false;
    if (decode_bch31(bch31 ^ 0x5u, &corrected, &errors) != 1 || corrected != bch31 || errors != 2)
      return false;
    if (decode_bch31(bch31 ^ 0x15u, &corrected, &errors) != 2) return false;  // 3-bit: rejected
  }

  // Full-chain replay: encode a synthetic batch (one alphanumeric page,
  // capcode 1234567, text "TEST") at 1200 baud, normal polarity, and confirm
  // the decoder reconstructs it exactly from discriminator samples alone.
  {
    // Frame number (low 3 bits of the capcode) must leave enough remaining
    // batch slots for the message's codewords to fit without spanning a
    // batch boundary -- this vector deliberately uses frame 0 for that
    // reason; multi-batch continuation is a separate, not-yet-covered case.
    constexpr uint32_t kCapcode = 1234560u;
    constexpr uint16_t kFrameNumber = kCapcode & 0x7u;
    const uint32_t address18 = kCapcode >> 3;
    const uint32_t address_info = (0u << 20) | (address18 << 2) | 0x3u;  // function 3 = alpha
    const uint32_t address_word = pocsag_encode_codeword(address_info);

    // "TEST" as 7-bit ASCII, LSB-first per character (bit-reversed), packed
    // MSB-first into 20-bit codewords -- the exact inverse of
    // Decoder::append_alpha_payload, so this is a genuine round-trip check.
    const char text[] = "TEST";
    uint64_t bitstream = 0;
    int bitstream_len = 0;
    for (char c : text) {
      if (c == '\0') break;
      const uint8_t reversed = reverse_bits(static_cast<uint8_t>(c) & 0x7Fu, 7);
      bitstream = (bitstream << 7) | reversed;
      bitstream_len += 7;
    }
    while (bitstream_len % 20 != 0) {
      bitstream <<= 1;
      ++bitstream_len;
    }
    uint32_t message_words[4];
    int message_word_count = bitstream_len / 20;
    for (int w = 0; w < message_word_count; ++w) {
      const int shift = bitstream_len - 20 * (w + 1);
      const uint32_t data20 = static_cast<uint32_t>((bitstream >> shift) & 0xFFFFFu);
      const uint32_t info = (1u << 20) | data20;
      message_words[w] = pocsag_encode_codeword(info);
    }

    // 64 preamble bits + 2 sync words + 16 codewords, all at 32
    // samples/symbol (1200 baud) = (64 + 32*18) * 32 = 20480 samples.
    // Heap-allocated (not `static`): a `static` buffer here would reserve
    // this permanently in BSS for the life of the program merely to run a
    // one-time boot self-check, which is what overflowed this firmware's
    // already-tight RAM budget the first time this was written that way.
    std::unique_ptr<float[]> samples(new float[21000]);
    TestVectorBuilder builder{samples.get(), 21000, 0, 32 /* 1200 baud */, false};
    builder.push_preamble(64);
    builder.push_word(kSyncWord);
    for (uint16_t slot = 0; slot < kBatchCodewords; ++slot) {
      if (slot == kFrameNumber * 2) {
        builder.push_word(address_word);
      } else if (slot > kFrameNumber * 2 &&
                 slot <= kFrameNumber * 2 + message_word_count) {
        builder.push_word(message_words[slot - kFrameNumber * 2 - 1]);
      } else {
        builder.push_word(kIdleWord);
      }
    }
    builder.push_word(kSyncWord);  // following batch's sync, to force finalize

    Decoder decoder;
    decoder.configure(Baud::auto_detect, Polarity::auto_detect);
    struct Captured {
      bool got = false;
      Message msg{};
    } captured;
    for (size_t i = 0; i < builder.count; ++i) {
      decoder.process_discriminator_sample(
          samples[i],
          [](const Message& m, void* ctx) {
            auto* c = static_cast<Captured*>(ctx);
            c->got = true;
            c->msg = m;
          },
          &captured);
    }
    if (!captured.got) return false;
    if (captured.msg.capcode != kCapcode) return false;
    if (captured.msg.type != MessageType::alpha) return false;
    if (captured.msg.baud != 1200) return false;
    if (std::strncmp(captured.msg.text, "TEST", 4) != 0) return false;
    if (decoder.stats().codewords_uncorrectable != 0) return false;
  }

  // Numeric round-trip and inverted-polarity detection, single check.
  {
    constexpr uint32_t kCapcode = 42u;
    constexpr uint16_t kFrameNumber = kCapcode & 0x7u;
    const uint32_t address18 = kCapcode >> 3;
    const uint32_t address_info = (0u << 20) | (address18 << 2) | 0x0u;  // numeric
    const uint32_t address_word = pocsag_encode_codeword(address_info);

    // Digits "123-U" packed as 5 reversed nibbles into one 20-bit codeword.
    const char digits[5] = {'1', '2', '3', '-', 'U'};
    uint32_t data20 = 0;
    for (int i = 0; i < 5; ++i) {
      const uint8_t nibble = numeric_char_to_nibble(digits[i]);
      const uint8_t reversed = reverse_bits(nibble, 4);
      data20 = (data20 << 4) | reversed;
    }
    const uint32_t message_word = pocsag_encode_codeword((1u << 20) | data20);

    // (64 + 32*18) * 16 = 10240 samples at 2400 baud.
    std::unique_ptr<float[]> samples(new float[11000]);
    TestVectorBuilder builder{samples.get(), 11000, 0, 16 /* 2400 baud */, true /* inverted */};
    builder.push_preamble(64);
    builder.push_word(kSyncWord);
    for (uint16_t slot = 0; slot < kBatchCodewords; ++slot) {
      if (slot == kFrameNumber * 2)
        builder.push_word(address_word);
      else if (slot == kFrameNumber * 2 + 1)
        builder.push_word(message_word);
      else
        builder.push_word(kIdleWord);
    }
    builder.push_word(kSyncWord);

    Decoder decoder;
    decoder.configure(Baud::auto_detect, Polarity::auto_detect);
    struct Captured {
      bool got = false;
      Message msg{};
    } captured;
    for (size_t i = 0; i < builder.count; ++i) {
      decoder.process_discriminator_sample(
          samples[i],
          [](const Message& m, void* ctx) {
            auto* c = static_cast<Captured*>(ctx);
            c->got = true;
            c->msg = m;
          },
          &captured);
    }
    if (!captured.got) return false;
    if (captured.msg.capcode != kCapcode) return false;
    if (captured.msg.type != MessageType::numeric) return false;
    if (captured.msg.baud != 2400) return false;
    if (!captured.msg.inverted) return false;
    if (std::strncmp(captured.msg.text, "123-U", 5) != 0) return false;
  }

  return true;
}

}  // namespace orcsdr::pocsag
