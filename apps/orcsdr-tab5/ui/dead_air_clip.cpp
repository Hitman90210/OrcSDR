#include "dead_air_clip.hpp"

#include <algorithm>
#include <cstring>

namespace orcsdr::dead_air {
namespace {
void put16(uint8_t* p, uint16_t v) { p[0] = static_cast<uint8_t>(v); p[1] = static_cast<uint8_t>(v >> 8); }
void put32(uint8_t* p, uint32_t v) {
  p[0] = static_cast<uint8_t>(v); p[1] = static_cast<uint8_t>(v >> 8);
  p[2] = static_cast<uint8_t>(v >> 16); p[3] = static_cast<uint8_t>(v >> 24);
}
}  // namespace

void ClipPlanner::configure(uint32_t rate_sps, uint32_t pre_ms, uint32_t post_ms) {
  rate_ = rate_sps;
  pre_ = static_cast<uint32_t>(static_cast<uint64_t>(rate_sps) * pre_ms / 1000u);
  post_ = static_cast<uint32_t>(static_cast<uint64_t>(rate_sps) * post_ms / 1000u);
  state_ = ClipState::idle;
}

ClipReject ClipPlanner::trigger(uint64_t written, uint64_t run_start, bool audio_flowing, uint32_t event_seq,
                                uint32_t now_ms, uint32_t unix_time) {
  if (state_ == ClipState::writing) return ClipReject::busy;
  if (state_ == ClipState::capturing) {  // join the clip already being captured
    ++job_.trigger_count;
    if (job_.seq_count < kClipMaxTriggers) job_.trigger_seqs[job_.seq_count++] = event_seq;
    return ClipReject::none;
  }
  if (!audio_flowing || written <= run_start) return ClipReject::no_audio;
  job_ = ClipJob{};
  job_.trigger_sample = written;
  job_.start = std::max<uint64_t>(run_start, written > pre_ ? written - pre_ : 0);
  job_.end = written + post_;
  job_.trigger_ms = now_ms;
  job_.unix_time = unix_time;
  job_.trigger_seqs[0] = event_seq;
  job_.seq_count = 1;
  job_.trigger_count = 1;
  run_at_trigger_ = run_start;
  gap_at_ = 0;
  state_ = ClipState::capturing;
  return ClipReject::none;
}

bool ClipPlanner::poll(uint64_t written, uint64_t run_start, bool audio_flowing, uint32_t now_ms, ClipJob* out,
                       ClipFail* fail) {
  if (fail) *fail = ClipFail::none;
  if (state_ != ClipState::capturing) return false;
  const uint64_t min_samples = static_cast<uint64_t>(rate_) * kClipMinMs / 1000u;
  // An interruption after the trigger: the stream restarted, so the run start moved past where we began.
  if (gap_at_ == 0 && run_start > run_at_trigger_) gap_at_ = run_start;
  uint64_t end = job_.end;
  bool truncated = false;
  bool done = written >= job_.end;
  if (gap_at_ != 0 && gap_at_ < end) { end = gap_at_; truncated = true; done = true; }
  if (!done && now_ms - job_.trigger_ms > (static_cast<uint64_t>(post_) * 1000u / rate_) + kClipStallMs) {
    end = std::min<uint64_t>(written, job_.end);  // the audio stopped arriving: keep what exists
    truncated = true;
    done = true;
  }
  (void)audio_flowing;
  if (!done) return false;
  if (end <= job_.start || end - job_.start < min_samples) {
    state_ = ClipState::idle;
    if (fail) *fail = ClipFail::audio_stopped;
    if (out) *out = job_;  // so the failure can name the events it was for
    return false;
  }
  job_.end = end;
  job_.truncated = truncated;
  if (out) *out = job_;
  state_ = ClipState::writing;
  return true;
}

void build_wav_header(uint8_t* out, uint32_t samples, uint32_t rate) {
  const uint32_t data_bytes = samples * 2u;
  std::memcpy(out, "RIFF", 4);
  put32(out + 4, 36u + data_bytes);
  std::memcpy(out + 8, "WAVEfmt ", 8);
  put32(out + 16, 16);          // PCM format chunk size
  put16(out + 20, 1);           // PCM
  put16(out + 22, 1);           // mono
  put32(out + 24, rate);
  put32(out + 28, rate * 2u);   // byte rate
  put16(out + 32, 2);           // block align
  put16(out + 34, 16);          // bits per sample
  std::memcpy(out + 36, "data", 4);
  put32(out + 40, data_bytes);
}

}  // namespace orcsdr::dead_air
