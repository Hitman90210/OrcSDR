#pragma once

#include <cstddef>
#include <cstdint>

// Hardware-independent POCSAG pager receiver: FSK demodulation, symbol
// timing/baud detection, BCH(31,21) forward error correction, and batch/
// codeword framing. No FreeRTOS, M5GFX, USB, SD, or network dependency --
// host-testable with synthetic IQ or a pre-demodulated discriminator stream,
// matching the p25_decoder_core / adsb_rx::Decoder precedent in this repo.
namespace orcsdr::pocsag {

// Input sample rate the built-in decimator/discriminator front end expects.
// Matches the existing hardware-verified RTL front end (960 kS/s CU8);
// 960000 / kInternalSampleRateHz == 25, an exact two-stage (5x5) CIC
// decimation ratio with no fractional residue.
constexpr uint32_t kInputSampleRateHz = 960000;
// Post-decimation discriminator rate. Chosen so all three standard POCSAG
// baud rates land on an exact integer samples-per-symbol count:
// 38400/512=75, 38400/1200=32, 38400/2400=16.
constexpr uint32_t kInternalSampleRateHz = 38400;

// Frame sync and idle codewords are protocol constants, not tuning knobs.
constexpr uint32_t kSyncWord = 0x7CD215D8u;
constexpr uint32_t kIdleWord = 0x7A89C197u;
constexpr uint16_t kBatchCodewords = 16;  // 8 frames x 2 codewords/frame

enum class Baud : uint8_t { auto_detect, b512, b1200, b2400 };
enum class Polarity : uint8_t { auto_detect, normal, inverted };
enum class MessageType : uint8_t { alpha, numeric, tone_only, unknown };

// Coarse acquisition state of the currently-winning (baud, polarity)
// candidate, if any. Reported for the SIGNAL dashboard.
enum class LockState : uint8_t {
  no_signal,
  searching,
  locked,
  lost,
};

// Bounded per spec: real POCSAG alphanumeric traffic is well under this in
// practice; a malformed/never-idle stream is truncated rather than allowed
// to grow this buffer further. Kept modest because this struct is embedded
// (by value) in bounded arrays elsewhere -- see Snapshot::messages.
constexpr size_t kMaxMessageChars = 96;

struct Message {
  uint32_t capcode = 0;
  uint8_t function = 0;
  MessageType type = MessageType::unknown;
  uint16_t baud = 0;
  bool inverted = false;
  bool truncated = false;
  uint16_t corrected_bits = 0;
  uint16_t uncorrectable_words = 0;
  char text[kMaxMessageChars] = {};
  uint16_t text_length = 0;
};

struct Stats {
  uint32_t codewords_total = 0;
  uint32_t codewords_valid = 0;
  uint32_t codewords_corrected = 0;
  uint32_t corrected_bit_count = 0;
  uint32_t codewords_uncorrectable = 0;
  uint32_t parity_failures = 0;
  uint32_t batches_synced = 0;
  uint32_t sync_losses = 0;
  uint32_t messages_decoded = 0;
  uint32_t messages_truncated = 0;

  LockState lock = LockState::no_signal;
  uint16_t detected_baud = 0;
  bool inverted = false;

  // Estimated from the FSK mark/space discriminator clusters once locked,
  // not a hardcoded constant.
  float fsk_deviation_hz = 0.0f;
  float frequency_offset_hz = 0.0f;

  // Bounded ring of recent normalized discriminator soft samples for the
  // SIGNAL dashboard's 2-FSK symbol-cluster plot. Fixed capacity.
  static constexpr size_t kSoftSymbolCapacity = 64;
  float soft_symbols[kSoftSymbolCapacity] = {};
  size_t soft_symbol_write = 0;
  size_t soft_symbol_count = 0;

  // Bounded ring of recent codeword outcomes (0=valid, 1=corrected,
  // 2=uncorrectable) for the SIGNAL dashboard's FEC-quality strip.
  static constexpr size_t kFecHistoryCapacity = 64;
  uint8_t fec_history[kFecHistoryCapacity] = {};
  size_t fec_history_write = 0;
  size_t fec_history_count = 0;
};

using MessageCallback = void (*)(const Message&, void*);

class Decoder {
 public:
  Decoder();

  void reset();
  // Manual overrides. Baud::auto_detect / Polarity::auto_detect (the
  // defaults after reset()) run every enabled candidate in parallel and
  // report whichever locks; a specific value restricts the search to that
  // one candidate only.
  void configure(Baud baud, Polarity polarity);

  // Primary entry point: raw CU8 IQ at kInputSampleRateHz. Runs the full
  // DSP chain (DC reject, two-stage CIC decimation, FM/FSK discriminator)
  // before symbol timing and framing.
  void process_cu8(const uint8_t* iq, size_t bytes, MessageCallback callback,
                    void* context);

  // Test/replay seam: one already-demodulated discriminator sample at
  // kInternalSampleRateHz, bypassing the IQ front end. Used by host tests
  // that exercise framing/BCH/charset logic directly, and available to a
  // future runtime path that taps an existing FM discriminator instead of
  // owning a second high-rate IQ stream (mirrors how RDS reuses the FM
  // demod tap rather than adding a parallel front end).
  void process_discriminator_sample(float sample, MessageCallback callback,
                                     void* context);

  const Stats& stats() const { return stats_; }
  static bool self_check();

 private:
  // One (baud, polarity) candidate's bit-synchronizer. Cheap: fixed-point
  // symbol-phase accumulator plus a 32-bit shift register for preamble/sync
  // correlation. No message-assembly state -- only the channel that wins
  // sync graduates to full batch/codeword decode, owned once at the
  // Decoder level (see below), not duplicated per channel.
  struct Channel {
    uint16_t samples_per_symbol = 0;
    bool inverted = false;
    uint16_t sample_counter = 0;
    float symbol_accum = 0.0f;
    uint16_t symbol_accum_count = 0;
    uint32_t bit_shift = 0;
    uint8_t transition_run = 0;
    bool last_bit = false;
    bool has_last_bit = false;
  };
  static constexpr size_t kChannelCount = 6;  // 3 bauds x 2 polarities

  // Feeds one discriminator sample to every enabled search channel; returns
  // true and sets *winner if one achieves sync-word correlation this call.
  bool search_channels(float sample, size_t* winner);
  // Feeds one discriminator sample to the active (locked) channel's bit
  // slicer; returns true and sets *bit when a new symbol decision lands.
  bool slice_active_channel(float sample, bool* bit);

  void on_codeword(uint32_t codeword32, MessageCallback callback, void* ctx);
  void finalize_message(MessageCallback callback, void* ctx);
  void record_soft_sample(float sample);
  void record_fec_outcome(uint8_t outcome);
  void append_alpha_payload(uint32_t data20);
  void append_numeric_payload(uint32_t data20);
  void append_char(char c);

  Baud configured_baud_ = Baud::auto_detect;
  Polarity configured_polarity_ = Polarity::auto_detect;
  Stats stats_{};

  Channel channels_[kChannelCount]{};
  int active_channel_ = -1;

  // Batch/codeword framing state, valid only while active_channel_ >= 0.
  uint16_t codeword_index_in_batch_ = 0;
  uint32_t codeword_accum_ = 0;
  uint8_t codeword_bit_count_ = 0;

  // In-progress message assembly, valid only while message_active_.
  bool message_active_ = false;
  uint32_t message_capcode_ = 0;
  uint8_t message_function_ = 0;
  MessageType message_type_ = MessageType::unknown;
  uint16_t message_codeword_count_ = 0;
  uint16_t message_corrected_bits_ = 0;
  uint16_t message_uncorrectable_words_ = 0;
  bool message_truncated_ = false;
  char message_text_[kMaxMessageChars]{};
  uint16_t message_text_length_ = 0;
  uint32_t alpha_bit_accum_ = 0;
  uint8_t alpha_bit_count_ = 0;

  // CIC decimator state (I/Q, two cascaded integrate-and-dump stages,
  // decimate-by-5 each == decimate-by-25 total: 960000/25 == 38400).
  struct CicStage {
    double integrator_i = 0.0;
    double integrator_q = 0.0;
    double previous_i = 0.0;
    double previous_q = 0.0;
    uint32_t phase = 0;
  };
  CicStage cic_stage1_{};
  CicStage cic_stage2_{};
  float dc_prev_i_ = 0.0f;
  float dc_prev_q_ = 0.0f;
  float dc_prev_y_i_ = 0.0f;
  float dc_prev_y_q_ = 0.0f;
  float discriminator_prev_i_ = 1.0f;
  float discriminator_prev_q_ = 0.0f;
  bool discriminator_primed_ = false;

  // Running mark/space cluster means for FSK deviation estimation, updated
  // once a channel is locked.
  float mark_mean_ = 0.5f;
  float space_mean_ = -0.5f;
};

}  // namespace orcsdr::pocsag
