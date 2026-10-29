#include "dead_air_journal.hpp"
#include <cassert>
#include <string>
#include <cstdio>
using namespace orcsdr::dead_air;
struct MemorySink : JournalSink {
  std::string contents;
  bool fail = false, synced = false;
  unsigned sync_calls = 0;
  bool append(const char* line, size_t len) override {
    if (fail) return false;
    contents.append(line, len); return true;
  }
  bool sync() override { synced = true; ++sync_calls; return !fail; }
};
static Event rf(uint32_t seq) {
  Event e{}; e.sequence = seq; e.monotonic_ms = seq * 10;
  e.frequency_hz = 100700000; e.source = Source::rf; e.kind = Kind::rf_rise;
  e.value = -65.0f; e.reference = -78.0f; e.threshold = 8.0f; return e;
}
static void encoding() {
  char line[320];
  Event e = rf(1);
  size_t len = encode_event_jsonl(e, line, sizeof(line));
  assert(len > 100 && line[len - 1] == '\n');
  std::string json(line, len);
  assert(json.find("\"unix_time\":null") != std::string::npos);
  assert(json.find("\"units\":\"dBFS\"") != std::string::npos);
  assert(json.find("\"baseline\":-78.000") != std::string::npos);
  assert(encode_event_jsonl(e, line, 10) == 0);
  Event m{}; m.sequence = 2; m.source = Source::manual;
  m.kind = Kind::manual_marker; m.unix_time = 1700000000;
  len = encode_event_jsonl(m, line, sizeof(line));
  assert(len > 0);
  json.assign(line, len);
  assert(json.find("\"value\":null") != std::string::npos);
  assert(json.find("\"unix_time\":1700000000") != std::string::npos);
}
static void queue_and_writer() {
  EventQueue q;
  for (uint32_t i = 1; i <= 64; ++i) assert(q.push(rf(i)));
  assert(!q.push(rf(65))); assert(q.dropped() == 1 && q.pending() == 64);
  MemorySink sink; EventJournal journal;
  assert(journal.start(sink, 1234));
  const unsigned sync_before = sink.sync_calls;
  assert(journal.drain(q, sink, 3) == 3);
  assert(sink.sync_calls == sync_before + 1);
  assert(journal.saved() == 3 && q.pending() == 61);
  assert(journal.finish(q, sink));
  assert(journal.saved() == 64 && journal.missing() == 0);
  assert(q.pending() == 0 && sink.synced);
  assert(sink.contents.find("\"type\":\"session_start\"") != std::string::npos);
  assert(sink.contents.find("\"seq\":64") != std::string::npos);
  assert(sink.contents.find("\"producer_dropped\":1") != std::string::npos);
  assert(sink.contents.find("\"type\":\"session_end\"") != std::string::npos);
}
static void gaps_and_listener() {
  EventQueue q; EventModel model;
  model.set_listener(enqueue_event, &q);
  model.mark_manual(123, 0, 101100000);
  assert(q.pending() == 1);
  Event got{};
  assert(q.pop(&got) && got.sequence == 1);
  assert(!q.pop(&got));
  assert(q.push(rf(2)) && q.push(rf(5)));
  MemorySink sink; EventJournal journal;
  assert(journal.start(sink, 44)); assert(journal.finish(q, sink));
  assert(journal.saved() == 2 && journal.missing() == 3);
  assert(sink.contents.find("\"first_seq\":1,\"last_seq\":1") != std::string::npos);
  assert(sink.contents.find("\"first_seq\":3,\"last_seq\":4") != std::string::npos);
}
static void failure() {
  EventQueue q; MemorySink sink; EventJournal journal;
  assert(!journal.start(sink, 0)); assert(journal.start(sink, 55));
  assert(q.push(rf(1))); sink.fail = true;
  assert(journal.drain(q, sink) == 0);
  assert(journal.failed() && journal.saved() == 0);
  assert(!journal.finish(q, sink));
}
// A flush that fails after a successful append must not count those events as saved (an SD card pulled mid-session).
struct FlushFailSink : MemorySink {
  bool fail_sync = false;
  bool sync() override { ++sync_calls; return !fail_sync; }
};
static void flush_failure() {
  EventQueue q; FlushFailSink sink; EventJournal journal;
  assert(journal.start(sink, 77));
  assert(q.push(rf(1)));
  assert(journal.drain(q, sink) == 1 && journal.saved() == 1 && journal.unconfirmed() == 0);
  sink.fail_sync = true;
  assert(q.push(rf(2)) && q.push(rf(3)));
  assert(journal.drain(q, sink) == 2);
  assert(journal.failed());
  assert(journal.saved() == 1 && journal.unconfirmed() == 2);   // confirmed vs appended-but-unflushed
  assert(journal.drain(q, sink) == 0 && journal.saved() == 1);   // nothing more is attempted after a failure
  assert(!journal.finish(q, sink));                               // and the session is never reported complete
}
int main() {
  encoding(); queue_and_writer(); gaps_and_listener(); failure(); flush_failure();
  std::puts("Dead Air journal tests PASS");
}
