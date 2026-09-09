#pragma once

// SAME / EAS header decoding for NOAA weather radio.
//
// NOAA Weather Radio prefixes every alert with a Specific Area Message Encoding
// burst: an AFSK header that names the originator, the event, the counties it
// applies to, and how long it is valid. Decoding it is the difference between
// "a robot voice is talking" and "there is a tornado warning for THIS county".
//
// Wire format (NWS/ITU-R BS.643, as used by NOAA):
//   * 520.833 bit/s AFSK, mark 2083.33 Hz = 1, space 1562.5 Hz = 0.
//     Both tones are integer multiples of the bit rate (4x and 3x), so a bit is
//     always a whole number of cycles -- which is what makes a short correlator
//     work as a demodulator.
//   * 8-bit ASCII, least significant bit first, no start/stop/parity. The bits
//     run continuously; there is no byte framing beyond the preamble.
//   * A burst is 16 bytes of 0xAB followed by the message text.
//   * Headers look like
//       ZCZC-ORG-EEE-PSSCCC-PSSCCC...+TTTT-JJJHHMM-LLLLLLLL-
//     and end-of-message bursts carry "NNNN".
//   * Every burst is transmitted three times. This decoder reports the first
//     clean copy rather than voting across all three: a header that parses and
//     carries a plausible event code is already strong evidence, and waiting
//     for agreement would drop an alert whose later copies were stepped on.
//
// Input is post-demodulation NFM audio, which on this device is 48 kHz mono
// int16 -- the same samples the speaker gets.

#include <cstddef>
#include <cstdint>

namespace orcsdr::same {

// Longest legal header is ZCZC + originator + event + 31 area codes + purge
// time + issue time + station, comfortably under this.
constexpr size_t kMaxMessageChars = 268;
constexpr size_t kMaxAreas = 31;

struct Message {
  char raw[kMaxMessageChars]{};  // the burst text, minus the preamble
  char originator[4]{};          // ORG: WXR, CIV, EAS, PEP
  char event[4]{};               // EEE: TOR, SVR, FFW, RWT...
  char purge[5]{};               // TTTT: valid duration, HHMM
  char issued[8]{};              // JJJHHMM: day of year and UTC issue time
  char station[9]{};             // LLLLLLLL: originating station
  uint32_t areas[kMaxAreas]{};   // PSSCCC, one per covered area
  uint8_t area_count = 0;
  bool end_of_message = false;   // an NNNN burst rather than a header
};

struct Stats {
  uint32_t preambles = 0;    // preamble runs locked onto
  uint32_t headers = 0;      // ZCZC bursts parsed
  uint32_t end_markers = 0;  // NNNN bursts seen
  uint32_t rejected = 0;     // bursts that locked but would not parse
};

class Decoder {
 public:
  void reset();

  // Post-demod audio, 48 kHz mono. Safe to call with any block size.
  void process(const int16_t* samples, size_t count);

  // Ends any burst still being assembled. A burst is normally terminated by
  // the first non-printable byte after it, which needs a full byte time of
  // carrier drop to arrive; call this when the caller knows the stream has
  // ended and should not be waited on.
  void flush();

  // Hands over one decoded message and clears it. Returns false when none is
  // waiting.
  bool take(Message* out);

  Stats stats() const { return stats_; }

  // True while the bit clock is locked to a preamble -- useful as a "receiving
  // an alert right now" indicator.
  bool locked() const { return locked_; }

  // Drives synthetic SAME audio through a real Decoder and checks the parse.
  static bool self_check();

  // The same checks, but naming the step that failed. There is no host build
  // for this firmware, so when the boot line says FAIL this is the only way to
  // learn which stage broke without a flash-and-guess cycle.
  static bool self_check_detail(char* detail, size_t detail_size);

 private:
  void push_bit(bool bit);
  void push_byte(uint8_t value);
  void finish_burst();
  bool parse(const char* text, Message* out) const;

  // Decimation 48 kHz -> 8 kHz.
  int32_t decim_accum_ = 0;
  uint8_t decim_count_ = 0;

  // Quadrature correlators for the two tones, integrated over about one bit.
  float mark_i_ = 0.0f, mark_q_ = 0.0f;
  float space_i_ = 0.0f, space_q_ = 0.0f;
  float mark_phase_ = 0.0f, space_phase_ = 0.0f;

  // Bit clock: a fractional accumulator, nudged by observed transitions.
  float bit_phase_ = 0.0f;
  float previous_soft_ = 0.0f;
  bool sampled_ = false;   // this bit already decided

  // Bit and byte assembly.
  uint32_t bit_run_ = 0;
  uint8_t byte_bits_ = 0;
  uint8_t byte_value_ = 0;
  bool locked_ = false;
  uint8_t preamble_run_ = 0;
  uint32_t shift_ = 0;

  char burst_[kMaxMessageChars]{};
  size_t burst_len_ = 0;

  Message pending_{};
  bool has_pending_ = false;
  Stats stats_{};
};

}  // namespace orcsdr::same
