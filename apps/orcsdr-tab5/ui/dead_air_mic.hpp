#pragma once
// Tab5 built-in microphone capture for Dead Air (optional, opt-in). The mic is a separate evidence source: its audio is
// never mixed into the receiver's audio, never kept anywhere but a short RAM ring unless an event makes a clip, and the
// owner starts and stops it explicitly. This module only runs the capture task and delivers 48 kHz mono PCM blocks to a
// sink; it knows nothing about clips or the screen.
#include <cstddef>
#include <cstdint>

namespace orcsdr::dead_air_mic {

constexpr uint32_t kSampleRate = 48000;

struct Stats {
  bool running = false;
  uint32_t blocks = 0;        // completed blocks delivered to the sink
  uint32_t start_failures = 0;
  float rms_dbfs = -100.0f;   // level of the last block
  float peak_dbfs = -100.0f;
  const char* error = nullptr;  // static string, set when start() fails
  uint32_t flat_blocks = 0;     // consecutive blocks whose samples were all identical (a stalled input, not a quiet room)
};

// Called on the mic task for every completed block. Must be fast and must not block.
using Sink = void (*)(const int16_t* pcm, size_t count);

// Starts capture. False (with Stats::error set) if the microphone could not be started; nothing is left running then.
bool start(Sink sink);
// Stops capture and releases the microphone. Safe to call when not running.
void stop();
bool running();
uint32_t flat_blocks();  // cheap; safe to call from the sink
Stats stats();

}  // namespace orcsdr::dead_air_mic
