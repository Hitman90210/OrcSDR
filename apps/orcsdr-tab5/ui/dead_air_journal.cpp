#include "dead_air_journal.hpp"
#include <cmath>
#include <cstdio>

namespace orcsdr::dead_air {
namespace {
const char* sensitivity_name(Sensitivity value) {
  switch (value) {
    case Sensitivity::quiet: return "quiet";
    case Sensitivity::sensitive: return "sensitive";
    default: return "standard";
  }
}
const char* units_for(const Event& event) {
  if (event.source == Source::rf || event.source == Source::audio) return "dBFS";
  switch (event.kind) {
    case Kind::temperature_change: return "C";
    case Kind::humidity_change: return "percent_RH";
    case Kind::pressure_change: return "hPa";
    default: return "";
  }
}
bool complete(int len, size_t capacity) {
  return len > 0 && static_cast<size_t>(len) < capacity;
}
} // namespace

bool EventQueue::push(const Event& event) {
  const uint32_t head = head_.load(std::memory_order_relaxed);
  const uint32_t tail = tail_.load(std::memory_order_acquire);
  if (head - tail >= kCapacity) {
    dropped_.fetch_add(1, std::memory_order_relaxed);
    return false;
  }
  slots_[head % kCapacity] = event;
  head_.store(head + 1, std::memory_order_release);
  return true;
}
bool EventQueue::pop(Event* event) {
  if (!event) return false;
  const uint32_t tail = tail_.load(std::memory_order_relaxed);
  if (tail == head_.load(std::memory_order_acquire)) return false;
  *event = slots_[tail % kCapacity];
  tail_.store(tail + 1, std::memory_order_release);
  return true;
}
size_t EventQueue::pending() const {
  const uint32_t head = head_.load(std::memory_order_acquire);
  const uint32_t tail = tail_.load(std::memory_order_acquire);
  return static_cast<size_t>(head - tail);
}
void enqueue_event(void* context, const Event& event) {
  if (context) static_cast<EventQueue*>(context)->push(event);
}
size_t encode_event_jsonl(const Event& event, char* output, size_t capacity) {
  if (!output || capacity == 0 || event.sequence == 0) return 0;
  char time_field[24];
  if (event.unix_time) {
    const int n = std::snprintf(time_field, sizeof(time_field), "%lu",
                                static_cast<unsigned long>(event.unix_time));
    if (!complete(n, sizeof(time_field))) return 0;
  } else {
    std::snprintf(time_field, sizeof(time_field), "null");
  }
  const char* source = source_name(event.source);
  const char* kind = kind_name(event.kind);
  if (event.source == Source::sweep) {
    // A generated tuning schedule, recorded so a reviewer can tell stepped listening from a fixed channel. The
    // settle value is the assumed (not measured) wait before a hop counts as observed.
    const int m = std::snprintf(output, capacity,
        "{\"v\":1,\"type\":\"event\",\"seq\":%lu,\"mono_ms\":%lu,\"unix_time\":%s,"
        "\"source\":\"%s\",\"kind\":\"%s\",\"frequency_hz\":%lu,"
        "\"sweep\":{\"end_hz\":%lu,\"step_hz\":%lu,\"dwell_ms\":%lu,\"settle_ms_assumed\":%lu,"
        "\"hops\":%lu,\"unobserved_ms\":%lu}}\n",
        static_cast<unsigned long>(event.sequence),
        static_cast<unsigned long>(event.monotonic_ms), time_field, source, kind,
        static_cast<unsigned long>(event.frequency_hz),
        static_cast<unsigned long>(event.sweep_end_hz), static_cast<unsigned long>(event.sweep_step_hz),
        static_cast<unsigned long>(event.sweep_dwell_ms), static_cast<unsigned long>(event.sweep_settle_ms),
        static_cast<unsigned long>(event.sweep_hops), static_cast<unsigned long>(event.sweep_unobserved_ms));
    return complete(m, capacity) ? static_cast<size_t>(m) : 0;
  }
  const int n = event.source == Source::manual && event.kind == Kind::manual_marker
      ? std::snprintf(output, capacity,
          "{\"v\":1,\"type\":\"event\",\"seq\":%lu,\"mono_ms\":%lu,\"unix_time\":%s,"
          "\"source\":\"manual\",\"kind\":\"manual_marker\",\"frequency_hz\":%lu,"
          "\"value\":null,\"units\":null,\"baseline\":null,\"threshold\":null}\n",
          static_cast<unsigned long>(event.sequence),
          static_cast<unsigned long>(event.monotonic_ms), time_field,
          static_cast<unsigned long>(event.frequency_hz))
      : (std::isfinite(event.value) && std::isfinite(event.reference) &&
         std::isfinite(event.threshold) && event.threshold > 0.0f
         ? std::snprintf(output, capacity,
          "{\"v\":1,\"type\":\"event\",\"seq\":%lu,\"mono_ms\":%lu,\"unix_time\":%s,"
          "\"source\":\"%s\",\"kind\":\"%s\",\"frequency_hz\":%lu,"
          "\"value\":%.3f,\"units\":\"%s\",\"baseline\":%.3f,\"threshold\":%.3f,"
          "\"sensitivity\":\"%s\"}\n",
          static_cast<unsigned long>(event.sequence),
          static_cast<unsigned long>(event.monotonic_ms), time_field,
          source, kind, static_cast<unsigned long>(event.frequency_hz),
          static_cast<double>(event.value), units_for(event),
          static_cast<double>(event.reference), static_cast<double>(event.threshold),
          sensitivity_name(event.sensitivity))
         : -1);
  return complete(n, capacity) ? static_cast<size_t>(n) : 0;
}
bool EventJournal::start(JournalSink& sink, uint32_t session_id) {
  if (started_ || failed_ || session_id == 0) return false;
  char line[128];
  const int n = std::snprintf(line, sizeof(line),
      "{\"v\":1,\"type\":\"session_start\",\"session_id\":%lu}\n",
      static_cast<unsigned long>(session_id));
  if (!complete(n, sizeof(line)) || !sink.append(line, static_cast<size_t>(n)) ||
      !sink.sync()) {
    failed_ = true;
    return false;
  }
  started_ = true; session_id_ = session_id;
  return true;
}
bool EventJournal::append_gap(JournalSink& sink, uint32_t first, uint32_t last) {
  char line[150];
  const int n = std::snprintf(line, sizeof(line),
      "{\"v\":1,\"type\":\"lost_events\",\"first_seq\":%lu,\"last_seq\":%lu,"
      "\"reason\":\"queue_overflow_or_discontinuity\"}\n",
      static_cast<unsigned long>(first), static_cast<unsigned long>(last));
  if (!complete(n, sizeof(line)) || !sink.append(line, static_cast<size_t>(n))) return false;
  missing_ += last - first + 1u;
  return true;
}
size_t EventJournal::drain(EventQueue& queue, JournalSink& sink, size_t max_records) {
  if (!started_ || failed_) return 0;
  size_t processed = 0;
  while (processed < max_records) {
    if (!pending_) pending_ = queue.pop(&in_flight_);
    if (!pending_) break;
    if (in_flight_.sequence <= last_sequence_) {
      failed_ = true; break; // Never silently reorder/replay records
    }
    if (in_flight_.sequence > last_sequence_ + 1u &&
        !append_gap(sink, last_sequence_ + 1u, in_flight_.sequence - 1u)) {
      failed_ = true; break;
    }
    char line[320];
    const size_t len = encode_event_jsonl(in_flight_, line, sizeof(line));
    if (len == 0 || !sink.append(line, len)) {
      failed_ = true; break;
    }
    last_sequence_ = in_flight_.sequence;
    ++unsynced_; ++processed;
    pending_ = false;
  }
  // Persist each bounded worker batch; no SD calls run in the UI/IQ task.
  if (processed != 0 && !failed_) {
    if (sink.sync()) { saved_ += unsynced_; unsynced_ = 0; }
    else failed_ = true;  // these events stay "unconfirmed": they may or may not have reached the card
  }
  return processed;
}
bool EventJournal::finish(EventQueue& queue, JournalSink& sink) {
  if (!started_ || failed_) return false;
  while (pending_ || queue.pending() > 0) {
    if (drain(queue, sink, 32) == 0) return false;
  }
  char line[192];
  const int n = std::snprintf(line, sizeof(line),
      "{\"v\":1,\"type\":\"session_end\",\"session_id\":%lu,"
      "\"saved\":%lu,\"lost\":%lu,\"producer_dropped\":%lu}\n",
      static_cast<unsigned long>(session_id_), static_cast<unsigned long>(saved_),
      static_cast<unsigned long>(missing_),
      static_cast<unsigned long>(queue.dropped()));
  if (!complete(n, sizeof(line)) || !sink.append(line, static_cast<size_t>(n)) ||
      !sink.sync()) {
    failed_ = true; return false;
  }
  started_ = false;
  return true;
}

} // namespace orcsdr::dead_air
