#include "same_decoder.hpp"

#include <cmath>
#include <cstdlib>
#include <cstring>

namespace orcsdr::same {
namespace {

// Audio arrives at 48 kHz; SAME lives between 1.5 and 2.1 kHz, so decimating by
// six to 8 kHz keeps both tones far below Nyquist and cuts the correlator work
// by the same factor.
constexpr int kDecimation = 6;
constexpr float kSampleRate = 8000.0f;
constexpr float kHostRate = 48000.0f;
constexpr float kBitRate = 520.833333f;
constexpr float kMarkHz = 2083.333333f;   // binary 1, 4x the bit rate
constexpr float kSpaceHz = 1562.5f;       // binary 0, 3x the bit rate
constexpr float kBitsPerSample = kBitRate / kSampleRate;
constexpr float kTwoPi = 6.283185307179586f;

// One-pole smoothing over roughly one bit. Long enough to separate tones
// 521 Hz apart, short enough that a transition is not smeared into its
// neighbour.
constexpr float kToneAlpha = 0.12f;

// How hard a transition pulls the bit clock. Small: the preamble gives a
// transition nearly every bit, so a gentle correction still locks quickly and
// does not chase noise through the long runs inside message text.
constexpr float kClockPull = 0.02f;

// 0xAB is the preamble byte. Sent LSB first it is 1,1,0,1,0,1,0,1 -- alternating
// enough to sync a bit clock on, which is exactly why it was chosen.
constexpr uint8_t kPreambleByte = 0xAB;
constexpr uint8_t kPreambleRunNeeded = 8;  // of the 16 sent; tolerate a late lock

bool ascii_printable(char c) { return c >= 0x20 && c <= 0x7e; }

}  // namespace

void Decoder::reset() {
  decim_accum_ = 0;
  decim_count_ = 0;
  mark_i_ = mark_q_ = space_i_ = space_q_ = 0.0f;
  mark_phase_ = space_phase_ = 0.0f;
  bit_phase_ = 0.0f;
  previous_soft_ = 0.0f;
  sampled_ = false;
  bit_run_ = 0;
  byte_bits_ = 0;
  byte_value_ = 0;
  locked_ = false;
  preamble_run_ = 0;
  shift_ = 0;
  burst_len_ = 0;
  burst_[0] = '\0';
  has_pending_ = false;
  pending_ = Message{};
}

void Decoder::process(const int16_t* samples, size_t count) {
  if (samples == nullptr) return;
  for (size_t index = 0; index < count; ++index) {
    decim_accum_ += samples[index];
    if (++decim_count_ < kDecimation) continue;
    const float sample =
        static_cast<float>(decim_accum_) / (kDecimation * 32768.0f);
    decim_accum_ = 0;
    decim_count_ = 0;

    // Quadrature-correlate against each tone and smooth. The reference phases
    // are accumulated and wrapped by subtraction rather than carried in a
    // running complex rotator, so a long alert cannot let repeated complex
    // multiplies walk the reference amplitude or frequency off the tone.
    mark_phase_ += kTwoPi * kMarkHz / kSampleRate;
    if (mark_phase_ > kTwoPi) mark_phase_ -= kTwoPi;
    space_phase_ += kTwoPi * kSpaceHz / kSampleRate;
    if (space_phase_ > kTwoPi) space_phase_ -= kTwoPi;

    mark_i_ += kToneAlpha * (sample * cosf(mark_phase_) - mark_i_);
    mark_q_ += kToneAlpha * (sample * sinf(mark_phase_) - mark_q_);
    space_i_ += kToneAlpha * (sample * cosf(space_phase_) - space_i_);
    space_q_ += kToneAlpha * (sample * sinf(space_phase_) - space_q_);

    const float mark_power = mark_i_ * mark_i_ + mark_q_ * mark_q_;
    const float space_power = space_i_ * space_i_ + space_q_ * space_q_;
    const float soft = mark_power - space_power;

    // Zero-crossing bit sync: a transition belongs on a bit boundary, so when
    // one arrives off-boundary, pull the clock toward it.
    if ((soft > 0.0f) != (previous_soft_ > 0.0f)) {
      const float error = bit_phase_ < 0.5f ? -bit_phase_ : (1.0f - bit_phase_);
      bit_phase_ += kClockPull * error;
      if (bit_phase_ < 0.0f) bit_phase_ += 1.0f;
    }
    previous_soft_ = soft;

    bit_phase_ += kBitsPerSample;
    if (bit_phase_ >= 1.0f) {
      bit_phase_ -= 1.0f;
      sampled_ = false;
    }
    // Decide in the middle of the bit, not at the boundary. The clock is being
    // pulled so transitions land on the boundary, which makes the boundary the
    // worst possible place to sample -- doing so decodes nothing at all.
    if (sampled_ || bit_phase_ < 0.5f) continue;
    sampled_ = true;
    push_bit(soft > 0.0f);
  }
}

void Decoder::push_bit(bool bit) {
  shift_ = (shift_ >> 1) | (bit ? 0x80000000u : 0u);
  ++bit_run_;

  if (!locked_) {
    // Hunt for whole preamble bytes on any bit phase. Once enough consecutive
    // 0xAB bytes have gone by, the byte boundary is wherever they ended.
    if (((shift_ >> 24) & 0xffu) == kPreambleByte && (bit_run_ % 8u) == 0u) {
      if (++preamble_run_ >= kPreambleRunNeeded) {
        locked_ = true;
        byte_bits_ = 0;
        byte_value_ = 0;
        burst_len_ = 0;
        ++stats_.preambles;
      }
      return;
    }
    // Re-anchor the byte phase on any preamble byte, so a lock can still form
    // no matter which bit of the burst was joined first.
    if (((shift_ >> 24) & 0xffu) == kPreambleByte) {
      bit_run_ = 0;
      preamble_run_ = 1;
    }
    return;
  }

  byte_value_ = static_cast<uint8_t>((byte_value_ >> 1) | (bit ? 0x80u : 0u));
  if (++byte_bits_ < 8) return;
  push_byte(byte_value_);
  byte_bits_ = 0;
  byte_value_ = 0;
}

void Decoder::push_byte(uint8_t value) {
  // Trailing preamble bytes before the text are skipped rather than treated as
  // content; the burst proper starts at the first printable character.
  if (value == kPreambleByte && burst_len_ == 0) return;
  if (!ascii_printable(static_cast<char>(value))) {
    // A non-printable byte ends the burst. A short run is noise, not a message.
    if (burst_len_ >= 4) {
      finish_burst();
    } else {
      burst_len_ = 0;
      locked_ = false;
      preamble_run_ = 0;
    }
    return;
  }
  if (burst_len_ + 1 >= sizeof(burst_)) {
    finish_burst();
    return;
  }
  burst_[burst_len_++] = static_cast<char>(value);
  burst_[burst_len_] = '\0';

  // NNNN is fixed length and self-terminating; recognising it lets an
  // end-of-message be reported without waiting for the carrier to drop.
  if (burst_len_ >= 4 && strncmp(burst_, "NNNN", 4) == 0) finish_burst();
}

void Decoder::finish_burst() {
  burst_[burst_len_] = '\0';
  Message message{};
  if (parse(burst_, &message)) {
    if (message.end_of_message) {
      ++stats_.end_markers;
    } else {
      ++stats_.headers;
    }
    pending_ = message;
    has_pending_ = true;
  } else if (burst_len_ >= 4) {
    ++stats_.rejected;
  }
  burst_len_ = 0;
  burst_[0] = '\0';
  locked_ = false;
  preamble_run_ = 0;
  byte_bits_ = 0;
  byte_value_ = 0;
}

bool Decoder::parse(const char* text, Message* out) const {
  if (text == nullptr || out == nullptr) return false;
  const size_t length = strlen(text);
  if (length >= kMaxMessageChars) return false;

  if (strncmp(text, "NNNN", 4) == 0) {
    *out = Message{};
    out->end_of_message = true;
    strncpy(out->raw, "NNNN", sizeof(out->raw) - 1);
    return true;
  }
  // ZCZC-ORG-EEE-PSSCCC...+TTTT-JJJHHMM-LLLLLLLL-
  if (length < 38 || strncmp(text, "ZCZC-", 5) != 0) return false;

  Message message{};
  strncpy(message.raw, text, sizeof(message.raw) - 1);

  const char* cursor = text + 5;
  auto take_field = [&cursor](char* destination, size_t size, char terminator) {
    const char* end = strchr(cursor, terminator);
    if (end == nullptr) return false;
    const size_t span = static_cast<size_t>(end - cursor);
    if (span == 0 || span >= size) return false;
    memcpy(destination, cursor, span);
    destination[span] = '\0';
    cursor = end + 1;
    return true;
  };

  if (!take_field(message.originator, sizeof(message.originator), '-')) return false;
  if (!take_field(message.event, sizeof(message.event), '-')) return false;

  // Area codes run until the '+' that introduces the purge time.
  const char* plus = strchr(cursor, '+');
  if (plus == nullptr) return false;
  while (cursor < plus && message.area_count < kMaxAreas) {
    char area[8]{};
    const char* end = static_cast<const char*>(
        memchr(cursor, '-', static_cast<size_t>(plus - cursor)));
    const char* stop = end != nullptr ? end : plus;
    const size_t span = static_cast<size_t>(stop - cursor);
    if (span == 0 || span >= sizeof(area)) return false;
    memcpy(area, cursor, span);
    area[span] = '\0';
    for (size_t i = 0; i < span; ++i)
      if (area[i] < '0' || area[i] > '9') return false;
    message.areas[message.area_count++] =
        static_cast<uint32_t>(strtoul(area, nullptr, 10));
    cursor = stop + 1;
    if (stop == plus) break;
  }
  if (message.area_count == 0) return false;
  cursor = plus + 1;

  if (!take_field(message.purge, sizeof(message.purge), '-')) return false;
  if (!take_field(message.issued, sizeof(message.issued), '-')) return false;
  // The station field is normally closed by a trailing '-'; accept
  // end-of-string too, because a burst clipped by squelch still carries
  // everything that matters.
  if (!take_field(message.station, sizeof(message.station), '-')) {
    const size_t span = strlen(cursor);
    if (span == 0 || span >= sizeof(message.station)) return false;
    memcpy(message.station, cursor, span);
    message.station[span] = '\0';
  }

  if (strlen(message.originator) != 3 || strlen(message.event) != 3) return false;
  if (strlen(message.purge) != 4 || strlen(message.issued) != 7) return false;
  *out = message;
  return true;
}

void Decoder::flush() {
  if (burst_len_ >= 4) finish_burst();
}

bool Decoder::take(Message* out) {
  if (!has_pending_ || out == nullptr) return false;
  *out = pending_;
  has_pending_ = false;
  return true;
}

namespace {

// Synthesises a SAME burst and streams it straight into a decoder.
//
// A whole burst is roughly 48,000 samples at 48 kHz. Holding that as a buffer
// costs about 96 KB, and internal RAM is the scarce resource on this device --
// the first version of this check did exactly that and overflowed DRAM at link
// time. Generating and consuming a block at a time costs 512 bytes instead.
class BurstFeeder {
 public:
  BurstFeeder(Decoder* decoder, float amplitude, float noise, uint32_t seed)
      : decoder_(decoder), amplitude_(amplitude), noise_(noise), state_(seed) {}

  void preamble() {
    for (int i = 0; i < 16; ++i) byte(kPreambleByte);
  }
  void text(const char* value) {
    for (; *value != '\0'; ++value) byte(static_cast<uint8_t>(*value));
  }
  void silence(size_t samples) {
    for (size_t i = 0; i < samples; ++i) push(0);
  }
  void flush() {
    if (used_ == 0) return;
    decoder_->process(block_, used_);
    used_ = 0;
  }

 private:
  void byte(uint8_t value) {
    for (int bit = 0; bit < 8; ++bit) emit_bit(((value >> bit) & 1u) != 0u);
  }

  void emit_bit(bool bit) {
    const float step = kTwoPi * (bit ? kMarkHz : kSpaceHz) / kHostRate;
    const size_t count = static_cast<size_t>(kHostRate / kBitRate + 0.5f);
    for (size_t i = 0; i < count; ++i) {
      phase_ += step;
      if (phase_ > kTwoPi) phase_ -= kTwoPi;
      float value = amplitude_ * sinf(phase_);
      if (noise_ > 0.0f) {
        state_ = state_ * 1664525u + 1013904223u;
        const float r =
            static_cast<float>((state_ >> 8) & 0xffffu) / 32768.0f - 1.0f;
        value += noise_ * r;
      }
      if (value > 1.0f) value = 1.0f;
      if (value < -1.0f) value = -1.0f;
      push(static_cast<int16_t>(value * 32000.0f));
    }
  }

  void push(int16_t sample) {
    block_[used_++] = sample;
    if (used_ == kBlock) flush();
  }

  static constexpr size_t kBlock = 256;
  Decoder* decoder_;
  float amplitude_;
  float noise_;
  uint32_t state_;
  float phase_ = 0.0f;
  int16_t block_[kBlock]{};
  size_t used_ = 0;
};

// Feeds one complete burst -- preamble, text, trailing silence -- through a
// fresh decoder and hands back whatever it produced.
bool run_burst(const char* text, float amplitude, float noise, uint32_t seed,
               Message* out) {
  Decoder decoder;
  decoder.reset();
  BurstFeeder feeder(&decoder, amplitude, noise, seed);
  feeder.preamble();
  feeder.text(text);
  // A burst ends on the first non-printable byte, which needs a whole byte
  // time of carrier drop -- 400 samples is 4.3 bits and decoded nothing.
  feeder.silence(1200);
  feeder.flush();
  decoder.flush();
  return decoder.take(out);
}

}  // namespace

bool Decoder::self_check() {
  char ignored[64];
  return self_check_detail(ignored, sizeof(ignored));
}

bool Decoder::self_check_detail(char* detail, size_t detail_size) {
  auto fail = [&](const char* step) {
    if (detail != nullptr && detail_size > 0) {
      strncpy(detail, step, detail_size - 1);
      detail[detail_size - 1] = ' ';
    }
    return false;
  };
  if (detail != nullptr && detail_size > 0) detail[0] = ' ';

  // The shape of a real NWS tornado warning: two Virginia county FIPS codes,
  // 45 minutes of validity, issued on day 253 at 18:00 UTC.
  static const char kHeader[] =
      "ZCZC-WXR-TOR-051153-051059+0045-2531800-KLWX/NWS-";

  // Clean signal: this is the algorithm test.
  Message message{};
  if (!run_burst(kHeader, 0.8f, 0.0f, 12345u, &message)) return fail("clean_no_message");
  if (strcmp(message.originator, "WXR") != 0) return fail("clean_originator");
  if (strcmp(message.event, "TOR") != 0) return fail("clean_event");
  if (message.area_count != 2) return fail("clean_area_count");
  if (message.areas[0] != 51153u || message.areas[1] != 51059u) return fail("clean_areas");
  if (strcmp(message.purge, "0045") != 0) return fail("clean_purge");
  if (strcmp(message.issued, "2531800") != 0) return fail("clean_issued");
  if (strcmp(message.station, "KLWX/NWS") != 0) return fail("clean_station");
  if (message.end_of_message) return fail("clean_eom_flag");

  // A fifth of the amplitude with noise on top, which is closer to a
  // weak-signal alert than the clean case above.
  Message weak{};
  if (!run_burst(kHeader, 0.15f, 0.12f, 987u, &weak)) return fail("weak_no_message");
  if (strcmp(weak.event, "TOR") != 0) return fail("weak_event");
  if (weak.area_count != 2) return fail("weak_area_count");

  // End-of-message bursts must parse as such, not as a header.
  Message ending{};
  if (!run_burst("NNNN", 0.8f, 0.0f, 4242u, &ending)) return fail("eom_no_message");
  if (!ending.end_of_message) return fail("eom_flag");

  // Malformed headers must be rejected rather than half-parsed: a two-letter
  // originator, plain text, and a non-numeric FIPS code.
  Decoder parser;
  Message rejected{};
  if (parser.parse("ZCZC-WX-TOR-051153+0045-2531800-KLWX-", &rejected))
    return fail("reject_short_org");
  if (parser.parse("hello world", &rejected)) return fail("reject_text");
  if (parser.parse("ZCZC-WXR-TOR-05115X-051059+0045-2531800-KLWX-", &rejected))
    return fail("reject_bad_fips");
  return true;
}

}  // namespace orcsdr::same
