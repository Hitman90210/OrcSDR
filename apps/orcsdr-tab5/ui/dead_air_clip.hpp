#pragma once
// Event-linked audio clips for Dead Air: a window of the receiver's demodulated audio from before an event to after
// it, kept exactly as the demodulator produced it. This file is pure logic (no SD, no radio, no display) and is
// host-tested: it decides which stretch of a rolling PCM ring becomes a clip and when, and builds the WAV header.
//
// Rules it enforces:
//  - A clip never contains audio from before an interruption in the audio stream (a gap in demodulation), and never
//    reaches past one: the clip is cut at the gap and marked truncated.
//  - A trigger with no flowing audio produces no clip (and says why), never an empty or invented one.
//  - One clip at a time. Triggers during the capture window join the clip in progress; a trigger while the previous
//    clip is still being written is refused as busy.
#include <cstddef>
#include <cstdint>

namespace orcsdr::dead_air {

constexpr uint32_t kClipSampleRate = 48000;
constexpr uint32_t kClipPreMs = 5000;
constexpr uint32_t kClipPostMs = 5000;
constexpr uint32_t kClipRingMs = 12000;   // pre + post + slack, so the writer can finish while audio keeps arriving
constexpr uint32_t kClipStallMs = 3000;   // give up on the post window this long after it should have completed
constexpr uint32_t kClipMinMs = 1000;     // anything shorter is not worth keeping
constexpr size_t kClipMaxTriggers = 8;
constexpr size_t kWavHeaderBytes = 44;

enum class ClipState : uint8_t { idle, capturing, writing };
enum class ClipReject : uint8_t { none, no_audio, busy };
enum class ClipFail : uint8_t { none, audio_stopped };

struct ClipJob {
  uint64_t start = 0;  // first sample (absolute index since the ring started)
  uint64_t end = 0;    // one past the last sample
  bool truncated = false;
  uint32_t trigger_seqs[kClipMaxTriggers]{};
  uint8_t seq_count = 0;
  uint32_t trigger_count = 0;  // every event that joined, including ones past the listed few
  uint32_t trigger_ms = 0;
  uint32_t unix_time = 0;
  uint64_t trigger_sample = 0;
};

class ClipPlanner {
 public:
  void configure(uint32_t rate_sps, uint32_t pre_ms, uint32_t post_ms);
  // `written`: samples written to the ring so far. `run_start`: first sample of the current uninterrupted run of audio.
  ClipReject trigger(uint64_t written, uint64_t run_start, bool audio_flowing, uint32_t event_seq, uint32_t now_ms,
                     uint32_t unix_time);
  // Call every UI tick. Returns true once when a job is ready to write, or sets *fail when the capture was abandoned.
  bool poll(uint64_t written, uint64_t run_start, bool audio_flowing, uint32_t now_ms, ClipJob* out, ClipFail* fail);
  void finished() { state_ = ClipState::idle; }  // the writer is done, successfully or not
  void cancel() { state_ = ClipState::idle; }    // drop a capture that can no longer complete (session closing)
  const ClipJob& pending() const { return job_; } // the triggers of the capture in progress
  ClipState state() const { return state_; }
  uint32_t pre_samples() const { return pre_; }
  uint32_t post_samples() const { return post_; }
  static constexpr uint32_t kRingSlackMs = 2000;

 private:
  ClipState state_ = ClipState::idle;
  uint32_t rate_ = kClipSampleRate, pre_ = 0, post_ = 0;
  ClipJob job_{};
  uint64_t run_at_trigger_ = 0;
  uint64_t gap_at_ = 0;  // first interruption seen after the trigger (0 = none)
};

// Canonical 44-byte PCM WAV header for `samples` mono 16-bit samples.
void build_wav_header(uint8_t* out44, uint32_t samples, uint32_t rate_sps);

}  // namespace orcsdr::dead_air
