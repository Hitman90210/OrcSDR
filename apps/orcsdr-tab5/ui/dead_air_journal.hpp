#pragma once
#include "dead_air_model.hpp"
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace orcsdr::dead_air {

// UI-task producer -> SD-worker consumer. Neither operation performs I/O or
// allocates memory. Allocate this owner in PSRAM, not in internal static BSS.
class EventQueue {
 public:
  static constexpr uint32_t kCapacity = 64;
  bool push(const Event& event);
  bool pop(Event* event);
  uint32_t dropped() const { return dropped_.load(std::memory_order_relaxed); }
  size_t pending() const;
 private:
  Event slots_[kCapacity]{};
  std::atomic<uint32_t> head_{0};
  std::atomic<uint32_t> tail_{0};
  std::atomic<uint32_t> dropped_{0};
};

// Callback for EventModel::set_listener(). Only call on its producer/UI task.
void enqueue_event(void* context, const Event& event);

// Contract: append is all-or-nothing; if a partial write occurs, the sink must
// be marked failed and the active session abandoned for offline recovery.
// Neither the receiver nor the UI render path may perform disk I/O.
struct JournalSink {
  virtual ~JournalSink() = default;
  virtual bool append(const char* line, size_t length) = 0;
  virtual bool sync() = 0;
};

// Produces versioned JSON Lines. Unknown wall-clock timestamps are null. Manual
// marks contain null measured fields, never misleading numeric zeros.
size_t encode_event_jsonl(const Event& event, char* output, size_t capacity);

class EventJournal {
 public:
  bool start(JournalSink& sink, uint32_t session_id);
  // Run on a dedicated low-priority storage worker, NOT an IQ/DSP callback.
  size_t drain(EventQueue& queue, JournalSink& sink, size_t max_records = 8);
  bool finish(EventQueue& queue, JournalSink& sink);
  bool failed() const { return failed_; }
  // Events confirmed durable (appended AND flushed). Appended-but-unflushed events are counted separately so a failed
  // flush can never make the log look more complete than it is.
  uint32_t saved() const { return saved_; }
  uint32_t unconfirmed() const { return unsynced_; }
  uint32_t missing() const { return missing_; }
  uint32_t last_sequence() const { return last_sequence_; }
 private:
  bool append_gap(JournalSink& sink, uint32_t start_sequence, uint32_t end_sequence);
  bool started_ = false, failed_ = false, pending_ = false;
  Event in_flight_{};
  uint32_t saved_ = 0, unsynced_ = 0, missing_ = 0, last_sequence_ = 0;
  uint32_t session_id_ = 0;
};

} // namespace orcsdr::dead_air
