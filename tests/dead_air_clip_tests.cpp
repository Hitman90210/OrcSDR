#include "dead_air_clip.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>

using namespace orcsdr::dead_air;

constexpr uint64_t kS = 48000;  // one second of samples

static ClipPlanner make() {
  ClipPlanner p;
  p.configure(48000, 5000, 5000);
  return p;
}

int main() {
  // A normal clip: 20 s of audio, trigger at 20 s, 5 s before and 5 s after.
  {
    ClipPlanner p = make();
    ClipJob job; ClipFail fail;
    assert(p.trigger(20 * kS, 0, true, 7, 20000, 1700000000) == ClipReject::none);
    assert(p.state() == ClipState::capturing);
    assert(!p.poll(24 * kS, 0, true, 24000, &job, &fail) && fail == ClipFail::none);   // post window not complete
    assert(p.poll(25 * kS, 0, true, 25000, &job, &fail));
    assert(job.start == 15 * kS && job.end == 25 * kS && !job.truncated);
    assert(job.seq_count == 1 && job.trigger_seqs[0] == 7 && job.trigger_count == 1 && job.unix_time == 1700000000);
    assert(p.state() == ClipState::writing);
    assert(!p.poll(26 * kS, 0, true, 26000, &job, &fail));   // delivered once only
    assert(p.trigger(26 * kS, 0, true, 8, 26000, 0) == ClipReject::busy);   // still writing
    p.finished();
    assert(p.state() == ClipState::idle);
  }
  // Not enough history: the clip starts where the audio started, never before.
  {
    ClipPlanner p = make();
    ClipJob job; ClipFail fail;
    assert(p.trigger(2 * kS, 0, true, 1, 2000, 0) == ClipReject::none);
    assert(p.poll(7 * kS, 0, true, 7000, &job, &fail));
    assert(job.start == 0 && job.end == 7 * kS && !job.truncated);
  }
  // The audio was interrupted before the trigger: the clip starts at the resume point, not across the gap.
  {
    ClipPlanner p = make();
    ClipJob job; ClipFail fail;
    assert(p.trigger(20 * kS, 18 * kS, true, 1, 20000, 0) == ClipReject::none);
    assert(p.poll(25 * kS, 18 * kS, true, 25000, &job, &fail));
    assert(job.start == 18 * kS && job.end == 25 * kS && !job.truncated);
  }
  // Interrupted after the trigger: the clip is cut at the gap and marked truncated.
  {
    ClipPlanner p = make();
    ClipJob job; ClipFail fail;
    assert(p.trigger(20 * kS, 0, true, 1, 20000, 0) == ClipReject::none);
    assert(!p.poll(22 * kS, 0, true, 22000, &job, &fail));
    // the stream restarts: its run begins at sample 23 s of the ring, so the first run ended there
    assert(p.poll(24 * kS, 23 * kS, true, 24000, &job, &fail));
    assert(job.start == 15 * kS && job.end == 23 * kS && job.truncated);
  }
  // No flowing audio: no clip, with the reason.
  {
    ClipPlanner p = make();
    assert(p.trigger(10 * kS, 0, false, 1, 10000, 0) == ClipReject::no_audio);
    assert(p.trigger(0, 0, true, 1, 10000, 0) == ClipReject::no_audio);
    assert(p.state() == ClipState::idle);
  }
  // Triggers during the capture join the clip; the first few seqs are listed, all are counted.
  {
    ClipPlanner p = make();
    ClipJob job; ClipFail fail;
    assert(p.trigger(20 * kS, 0, true, 100, 20000, 0) == ClipReject::none);
    for (uint32_t i = 1; i < 12; ++i) assert(p.trigger(20 * kS + i * 1000, 0, true, 100 + i, 20000 + i, 0) == ClipReject::none);
    assert(p.poll(25 * kS, 0, true, 25000, &job, &fail));
    assert(job.trigger_count == 12 && job.seq_count == kClipMaxTriggers && job.trigger_seqs[7] == 107);
    assert(job.start == 15 * kS && job.end == 25 * kS);   // joining does not move the window
  }
  // Audio stops arriving mid-window: after the stall limit the clip keeps what exists, marked truncated.
  {
    ClipPlanner p = make();
    ClipJob job; ClipFail fail;
    assert(p.trigger(20 * kS, 0, true, 1, 20000, 0) == ClipReject::none);
    assert(!p.poll(23 * kS, 0, false, 23000, &job, &fail));
    assert(!p.poll(23 * kS, 0, false, 27999, &job, &fail));
    assert(p.poll(23 * kS, 0, false, 28001, &job, &fail));   // 5 s window + 3 s stall limit passed
    assert(job.start == 15 * kS && job.end == 23 * kS && job.truncated);
  }
  // Too little audio to keep: abandoned, reported, and the planner is free again.
  {
    ClipPlanner p = make();
    ClipJob job; ClipFail fail = ClipFail::none;
    assert(p.trigger(kS / 2, 0, true, 1, 500, 0) == ClipReject::none);
    assert(!p.poll(kS / 2, 0, false, 9000, &job, &fail));
    assert(fail == ClipFail::audio_stopped && p.state() == ClipState::idle);
  }
  // WAV header: canonical layout and sizes.
  {
    uint8_t h[kWavHeaderBytes];
    build_wav_header(h, 480000, 48000);
    assert(std::memcmp(h, "RIFF", 4) == 0 && std::memcmp(h + 8, "WAVEfmt ", 8) == 0 && std::memcmp(h + 36, "data", 4) == 0);
    const auto rd32 = [&](int o) { return h[o] | (h[o + 1] << 8) | (h[o + 2] << 16) | (static_cast<uint32_t>(h[o + 3]) << 24); };
    assert(rd32(4) == 36u + 960000u && rd32(40) == 960000u && rd32(24) == 48000u && rd32(28) == 96000u);
    assert(h[20] == 1 && h[22] == 1 && h[32] == 2 && h[34] == 16);
  }
  std::puts("Dead Air clip tests PASS");
}
