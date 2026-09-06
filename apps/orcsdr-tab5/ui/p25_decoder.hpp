#pragma once

#include "p25_decoder_core.hpp"

namespace orcsdr::p25decoder {

using p25core::Grant;
using p25core::Snapshot;
using p25core::VoiceFrame;
inline constexpr size_t kRecentGrantCount = p25core::kRecentGrantCount;
inline constexpr size_t kVoiceFrameBits = p25core::kVoiceFrameBits;

// Device adapter: the RTL delivery task is the single decoder writer. The UI
// snapshot and voice-task queue are synchronized here, outside the core.
void reset();
void process_cu8(const uint8_t* iq, size_t bytes);
void reset_at(uint32_t now_ms);
void suspend_voice();
void process_cu8_at(const uint8_t* iq, size_t bytes, uint32_t now_ms);
Snapshot snapshot();
bool pop_voice_frame(VoiceFrame* frame);
bool self_check();

}  // namespace orcsdr::p25decoder
