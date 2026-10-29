#include "dead_air_mic.hpp"

#include <M5Unified.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace orcsdr::dead_air_mic {
namespace {
constexpr size_t kBlockSamples = 960;  // 20 ms at 48 kHz
constexpr size_t kBuffers = 4;         // M5Unified keeps two in flight; the one queued two steps ago is complete

struct Shared {
  std::atomic<bool> run{false};
  std::atomic<bool> alive{false};
  std::atomic<bool> enabled{false};
  std::atomic<uint32_t> blocks{0};
  std::atomic<uint32_t> start_failures{0};
  std::atomic<uint32_t> flat{0};
  std::atomic<float> rms{-100.0f};
  std::atomic<float> peak{-100.0f};
  std::atomic<const char*> error{nullptr};
  Sink sink = nullptr;
  int16_t* buffers = nullptr;
  TaskHandle_t task = nullptr;
};
Shared g;

float to_dbfs(float fraction) { return fraction > 0.0f ? std::max(-100.0f, 20.0f * std::log10(fraction)) : -100.0f; }

void deliver(const int16_t* pcm) {
  int64_t sum = 0;
  int32_t peak = 0;
  int32_t lo = pcm[0], hi = pcm[0];
  for (size_t i = 0; i < kBlockSamples; ++i) {
    const int32_t v = pcm[i];
    lo = std::min(lo, v);
    hi = std::max(hi, v);
    sum += static_cast<int64_t>(v) * v;
    peak = std::max(peak, v < 0 ? -v : v);
  }
  g.rms.store(to_dbfs(std::sqrt(static_cast<float>(sum) / static_cast<float>(kBlockSamples)) / 32768.0f), std::memory_order_relaxed);
  g.peak.store(to_dbfs(static_cast<float>(peak) / 32768.0f), std::memory_order_relaxed);
  g.blocks.fetch_add(1, std::memory_order_relaxed);
  if (hi == lo) g.flat.fetch_add(1, std::memory_order_relaxed);
  else g.flat.store(0, std::memory_order_relaxed);
  if (g.sink) g.sink(pcm, kBlockSamples);
}

void mic_task(void*) {
  size_t index = 0, queued = 0;
  while (g.run.load(std::memory_order_acquire)) {
    if (M5.Mic.isRecording() < 2) {
      int16_t* buffer = g.buffers + index * kBlockSamples;
      if (M5.Mic.record(buffer, kBlockSamples, kSampleRate, false)) {
        if (queued >= 2) deliver(g.buffers + ((index + kBuffers - 2) % kBuffers) * kBlockSamples);
        ++queued;
        index = (index + 1) % kBuffers;
        continue;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(2));
  }
  M5.Mic.end();
  g.enabled.store(false, std::memory_order_release);
  g.alive.store(false, std::memory_order_release);
  vTaskDeleteWithCaps(nullptr);
}
}  // namespace

bool start(Sink sink) {
  if (g.alive.load(std::memory_order_acquire)) return true;  // already running
  g.error.store(nullptr, std::memory_order_relaxed);
  if (!g.buffers) {
    g.buffers = static_cast<int16_t*>(heap_caps_malloc(kBuffers * kBlockSamples * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!g.buffers) { g.error.store("no_memory", std::memory_order_relaxed); g.start_failures.fetch_add(1); return false; }
  }
  auto cfg = M5.Mic.config();
  cfg.sample_rate = kSampleRate;
  cfg.stereo = false;
  cfg.dma_buf_len = 256;
  cfg.dma_buf_count = 3;
  cfg.task_priority = 2;
  cfg.task_pinned_core = 0;  // the DSP runs on core 1; keep the mic off it
  M5.Mic.config(cfg);
  if (!M5.Mic.begin()) {
    g.error.store("mic_begin_failed", std::memory_order_relaxed);
    g.start_failures.fetch_add(1);
    return false;
  }
  g.enabled.store(true, std::memory_order_release);
  g.sink = sink;
  g.blocks.store(0, std::memory_order_relaxed);
  g.flat.store(0, std::memory_order_relaxed);
  g.run.store(true, std::memory_order_release);
  g.alive.store(true, std::memory_order_release);
  if (xTaskCreatePinnedToCoreWithCaps(mic_task, "dead_air_mic", 6144, nullptr, 2, &g.task, 0,
                                      MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) != pdPASS) {
    g.run.store(false, std::memory_order_release);
    g.alive.store(false, std::memory_order_release);
    M5.Mic.end();
    g.enabled.store(false, std::memory_order_release);
    g.error.store("task_failed", std::memory_order_relaxed);
    g.start_failures.fetch_add(1);
    return false;
  }
  return true;
}

void stop() {
  if (!g.alive.load(std::memory_order_acquire)) return;
  g.run.store(false, std::memory_order_release);
  for (int i = 0; i < 100 && g.alive.load(std::memory_order_acquire); ++i) vTaskDelay(pdMS_TO_TICKS(10));
  g.sink = nullptr;
}

bool running() { return g.alive.load(std::memory_order_acquire) && g.run.load(std::memory_order_acquire); }

uint32_t flat_blocks() { return g.flat.load(std::memory_order_relaxed); }

Stats stats() {
  Stats s;
  s.running = running();
  s.blocks = g.blocks.load(std::memory_order_relaxed);
  s.start_failures = g.start_failures.load(std::memory_order_relaxed);
  s.rms_dbfs = g.rms.load(std::memory_order_relaxed);
  s.peak_dbfs = g.peak.load(std::memory_order_relaxed);
  s.error = g.error.load(std::memory_order_relaxed);
  s.flat_blocks = g.flat.load(std::memory_order_relaxed);
  return s;
}

}  // namespace orcsdr::dead_air_mic
