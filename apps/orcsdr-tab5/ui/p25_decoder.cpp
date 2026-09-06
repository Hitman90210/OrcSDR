#include "p25_decoder.hpp"

#include <array>
#include <atomic>

#include "freertos/FreeRTOS.h"

namespace orcsdr::p25decoder {
namespace {

constexpr size_t kVoiceQueueDepth = 18;
std::array<VoiceFrame, kVoiceQueueDepth> g_voice_queue{};
std::atomic<uint8_t> g_voice_write{0};
std::atomic<uint8_t> g_voice_read{0};
Snapshot g_public_snapshot{};
portMUX_TYPE g_snapshot_mux = portMUX_INITIALIZER_UNLOCKED;

bool enqueue_voice(const VoiceFrame& frame, void*) {
  const uint8_t write = g_voice_write.load(std::memory_order_relaxed);
  const uint8_t next = static_cast<uint8_t>((write + 1) % kVoiceQueueDepth);
  if (next == g_voice_read.load(std::memory_order_acquire)) return false;
  g_voice_queue[write] = frame;
  g_voice_write.store(next, std::memory_order_release);
  return true;
}

void clear_voice_queue() {
  g_voice_read.store(g_voice_write.load(std::memory_order_acquire),
                     std::memory_order_release);
}

void publish_snapshot() {
  const Snapshot value = p25core::snapshot();
  portENTER_CRITICAL(&g_snapshot_mux);
  g_public_snapshot = value;
  portEXIT_CRITICAL(&g_snapshot_mux);
}

}  // namespace

void reset() {
  reset_at(millis());
}

void reset_at(uint32_t now_ms) {
  clear_voice_queue();
  p25core::reset(now_ms);
  publish_snapshot();
}

void process_cu8(const uint8_t* iq, size_t bytes) {
  process_cu8_at(iq, bytes, millis());
}

void process_cu8_at(const uint8_t* iq, size_t bytes, uint32_t now_ms) {
  p25core::process_cu8(iq, bytes, now_ms, enqueue_voice);
  publish_snapshot();
}

Snapshot snapshot() {
  Snapshot value;
  portENTER_CRITICAL(&g_snapshot_mux);
  value = g_public_snapshot;
  portEXIT_CRITICAL(&g_snapshot_mux);
  return value;
}

bool pop_voice_frame(VoiceFrame* frame) {
  if (frame == nullptr) return false;
  const uint8_t read = g_voice_read.load(std::memory_order_relaxed);
  if (read == g_voice_write.load(std::memory_order_acquire)) return false;
  *frame = g_voice_queue[read];
  g_voice_read.store(static_cast<uint8_t>((read + 1) % kVoiceQueueDepth),
                     std::memory_order_release);
  return true;
}

bool self_check() { return p25core::self_check(); }

}  // namespace orcsdr::p25decoder
