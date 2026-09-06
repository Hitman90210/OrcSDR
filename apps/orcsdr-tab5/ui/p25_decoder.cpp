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
std::atomic<uint32_t> g_voice_generation{0};
std::atomic<bool> g_voice_accepting{false};
Snapshot g_public_snapshot{};
portMUX_TYPE g_snapshot_mux = portMUX_INITIALIZER_UNLOCKED;

struct VoiceContext {
  uint32_t generation;
  bool accepting;
};

bool enqueue_voice(const VoiceFrame& frame, void* context) {
  const auto* voice = static_cast<const VoiceContext*>(context);
  if (!voice->accepting || !g_voice_accepting.load(std::memory_order_acquire) ||
      voice->generation != g_voice_generation.load(std::memory_order_acquire)) return true;
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
  g_voice_accepting.store(false, std::memory_order_release);
  clear_voice_queue();
  p25core::reset(now_ms);
  g_voice_generation.fetch_add(1, std::memory_order_acq_rel);
  g_voice_accepting.store(true, std::memory_order_release);
  publish_snapshot();
}

void suspend_voice() {
  g_voice_accepting.store(false, std::memory_order_release);
  g_voice_generation.fetch_add(1, std::memory_order_acq_rel);
  clear_voice_queue();
}

void process_cu8(const uint8_t* iq, size_t bytes) {
  process_cu8_at(iq, bytes, millis());
}

void process_cu8_at(const uint8_t* iq, size_t bytes, uint32_t now_ms) {
  VoiceContext voice{
      g_voice_generation.load(std::memory_order_acquire),
      g_voice_accepting.load(std::memory_order_acquire)};
  p25core::process_cu8(iq, bytes, now_ms, enqueue_voice, &voice);
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
