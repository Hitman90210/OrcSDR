#pragma once

#include <cstddef>
#include <cstdint>

namespace orcsdr::p25core {

constexpr size_t kRecentGrantCount = 8;
constexpr size_t kVoiceFrameBits = 144;
constexpr uint8_t kClearAlgorithmId = 0x80;

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

struct Grant {
  bool valid = false;
  bool encrypted = false;
  bool emergency = false;
  bool tdma = false;
  uint16_t talkgroup = 0;
  uint32_t source_id = 0;
  uint32_t frequency_hz = 0;
  uint32_t seen_ms = 0;
};

struct Snapshot {
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
  uint32_t voice_ldus = 0;
  uint32_t voice_frames = 0;
  uint32_t voice_queue_drops = 0;
  uint32_t last_voice_ms = 0;
  uint32_t encryption_sync_good = 0;
  uint32_t encryption_sync_failed = 0;
  EncryptionSync voice_encryption{};
  uint16_t last_trellis_metric = 0;
  float estimated_ber_percent = 0.0f;
  float frame_error_percent = 0.0f;
  float afc_offset_hz = 0.0f;
  float symbol_level = 0.0f;
  Grant current_grant{};
  Grant recent_grants[kRecentGrantCount]{};
};

using VoiceSink = bool (*)(const VoiceFrame& frame, void* context);

// Hardware-independent Phase I C4FM receiver. Callers supply the monotonic
// clock and an optional bounded voice-frame sink. The radio has one receiver,
// so the core owns one fixed-memory instance and performs no heap allocation.
void reset(uint32_t now_ms = 0);
void process_cu8(const uint8_t* iq, size_t bytes, uint32_t now_ms,
                 VoiceSink voice_sink = nullptr, void* voice_context = nullptr);
Snapshot snapshot();

// Decode the six 40-bit Encryption Sync fields from an LDU2 payload. The
// input is the 1,568 payload bits after the NID, stored as 784 dibits.
bool decode_ldu2_encryption(const uint8_t* payload, size_t dibits,
                            EncryptionSync* result);

// Deterministic protocol/FEC check. It does not touch receiver state.
bool self_check();

}  // namespace orcsdr::p25core
