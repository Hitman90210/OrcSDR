#pragma once

#include <cstddef>
#include <cstdint>

namespace orcsdr::p25core {

constexpr size_t kRecentGrantCount = 8;
constexpr size_t kVoiceFrameBits = 144;
constexpr uint8_t kClearAlgorithmId = 0x80;

// Phase I symbol-recovery path. Auto runs both paths only until one produces
// valid P25 protocol frames, then keeps the successful path until lock loss.
enum class Modulation : uint8_t {
  auto_detect,
  c4fm,
  cqpsk,
};

struct EncryptionSync {
  bool valid = false;
  bool encrypted = false;
  uint8_t algorithm_id = 0;
  uint16_t key_id = 0;
  uint8_t corrected_errors = 0;
};

struct VoiceFrame {
  uint8_t bits[kVoiceFrameBits]{};
  uint32_t sequence = 0;
  EncryptionSync encryption{};
};

struct ChannelAssignment {
  bool valid = false;
  uint32_t frequency_hz = 0;
  uint8_t slot = 0;
};

struct Grant {
  bool valid = false;
  bool encrypted = false;
  bool emergency = false;
  bool tdma = false;
  uint8_t service_options = 0;
  uint8_t channel_id = 0;
  uint16_t channel_number = 0;
  uint8_t slot = 0;
  uint16_t talkgroup = 0;
  uint32_t source_id = 0;
  uint32_t frequency_hz = 0;
  uint32_t wacn = 0;
  uint16_t system_id = 0;
  uint8_t rfss = 0;
  uint8_t site = 0;
  uint32_t seen_ms = 0;
};

struct Snapshot {
  Modulation configured_modulation = Modulation::auto_detect;
  Modulation selected_modulation = Modulation::auto_detect;
  bool frame_sync = false;
  bool identity_valid = false;
  uint16_t nac = 0;
  uint32_t wacn = 0;
  uint16_t system_id = 0;
  uint8_t rfss = 0;
  uint8_t site = 0;
  uint32_t sync_words = 0;
  uint32_t nid_good = 0;
  uint32_t nid_failed = 0;
  uint32_t nid_corrected_bits = 0;
  uint32_t tsbk_good = 0;
  uint32_t tsbk_failed = 0;
  uint32_t phase2_band_plans = 0;
  uint32_t phase2_grants = 0;
  uint32_t phase2_mapping_errors = 0;
  bool phase2_acquisition = false;
  uint32_t phase2_symbols = 0;
  uint32_t phase2_sync_words = 0;
  uint32_t phase2_last_sync_ms = 0;
  uint8_t phase2_best_sync_errors = 40;
  uint32_t phase2_complete_bursts = 0;
  uint32_t phase2_truncated_bursts = 0;
  uint32_t phase2_last_burst_ms = 0;
  uint8_t phase2_last_duid_codeword = 0;
  uint8_t phase2_last_duid = 0;
  uint8_t phase2_last_duid_errors = 0;
  bool phase2_last_duid_valid = false;
  uint32_t phase2_voice_bursts = 0;
  uint32_t phase2_control_bursts = 0;
  uint32_t phase2_unknown_bursts = 0;
  bool phase2_reverse_polarity = false;
  uint32_t voice_ldus = 0;
  uint32_t voice_frames = 0;
  uint32_t voice_queue_drops = 0;
  uint32_t voice_unrouted_frames = 0;
  uint32_t last_voice_ms = 0;
  uint32_t encryption_sync_good = 0;
  uint32_t encryption_sync_failed = 0;
  EncryptionSync voice_encryption{};
  uint16_t last_trellis_metric = 0;
  float estimated_ber_percent = 0.0f;
  float frame_error_percent = 0.0f;
  float afc_offset_hz = 0.0f;
  float symbol_level = 0.0f;
  float lock_quality_percent = 0.0f;
  float timing_error = 0.0f;
  float carrier_error_hz = 0.0f;
  float decode_rate_hz = 0.0f;
  Grant current_grant{};
  Grant recent_grants[kRecentGrantCount]{};
};

using VoiceSink = bool (*)(const VoiceFrame& frame, void* context);

// Hardware-independent Phase I C4FM/CQPSK receiver. Callers supply the
// monotonic clock and an optional bounded voice-frame sink. The radio has one
// receiver, so the core owns one fixed-memory instance and allocates no heap.
void reset(uint32_t now_ms = 0);
// Select the demodulator and CQPSK Gardner/Costas gains. Invalid/non-positive
// gains use the stable defaults (0.005 and 0.008).
void configure(Modulation modulation, float timing_gain, float carrier_gain);
void set_modulation(Modulation modulation);
Modulation modulation();
const char* modulation_name(Modulation modulation);
void process_cu8(const uint8_t* iq, size_t bytes, uint32_t now_ms,
                 VoiceSink voice_sink = nullptr, void* voice_context = nullptr);
void set_phase2_acquisition(bool enabled, uint32_t now_ms);
// Test/replay seam for the shared 48 kS/s complex channel stream.
void process_channel_iq(float i, float q, uint32_t now_ms);
// Deterministic replay/test seam for already-sliced Phase II dibits.
void process_phase2_dibit(uint8_t dibit, uint32_t now_ms);
Snapshot snapshot();

// Map a logical channel number to its physical RF carrier and TDMA slot.
// Phase II uses two logical channels per carrier; FDMA uses one.
ChannelAssignment map_channel(uint64_t base_hz, uint32_t spacing_hz,
                              uint8_t slots_per_carrier,
                              uint16_t channel_number);

// Decode the six 40-bit Encryption Sync fields from an LDU2 payload. The
// input is the 1,568 payload bits after the NID, stored as 784 dibits.
bool decode_ldu2_encryption(const uint8_t* payload, size_t dibits,
                            EncryptionSync* result);

// Deterministic protocol/FEC check. It does not touch receiver state.
bool self_check();

}  // namespace orcsdr::p25core
