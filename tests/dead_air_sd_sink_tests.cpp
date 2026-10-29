// Host contract test for OrcSDR storage::FileSystem-backed Dead Air writer.
// The fake below obeys the FileSystem API but doesn't require ESP-IDF.
#include "dead_air_sd_journal.hpp"
#include <cassert>
#include <cstring>
#include <memory>
#include <string>
#include <unordered_map>
#include <cstdio>
static std::unordered_map<std::string, std::string> disk;
static bool g_sd_mounted = true, partial_write = false;
namespace orcsdr::storage {
struct File::State { std::string path; };
File::File(std::shared_ptr<State> state) : state_(std::move(state)) {}
File::operator bool() const { return static_cast<bool>(state_); }
size_t File::write(const uint8_t* data, size_t len) {
  if (!state_) return 0;
  const size_t accepted = partial_write && len ? len - 1 : len;
  disk[state_->path].append(reinterpret_cast<const char*>(data), accepted);
  return accepted;
}
bool File::sync() { return state_ && g_sd_mounted; }
bool File::close() { if (!state_) return true; state_.reset(); return true; }
File FileSystem::open(const char* path, const char* mode, bool) const {
  if (!g_sd_mounted || !path || !mode || mode[0] != 'a') return {};
  auto state = std::make_shared<File::State>();
  state->path = path;
  disk.try_emplace(path, "");
  return File(std::move(state));
}
bool FileSystem::exists(const char* path) const { return disk.find(path) != disk.end(); }
bool FileSystem::mkdir(const char* path) const { return (std::strcmp(path, "/orcsdr") == 0 || std::strcmp(path, "/orcsdr/dead_air") == 0) && g_sd_mounted; }
bool mounted() { return ::g_sd_mounted; }
}
using namespace orcsdr::dead_air;
int main() {
  orcsdr::storage::FileSystem fs;
  SdJournalSink sink;
  assert(sink.open(fs, 123));
  assert(sink.session_id() == 123);
  assert(std::string(sink.path()) == "/orcsdr/dead_air/session_0000000123.jsonl");
  EventQueue queue;
  Event event{};
  event.sequence = 1;
  event.source = Source::manual;
  event.kind = Kind::manual_marker;
  event.monotonic_ms = 30;
  assert(queue.push(event));
  EventJournal journal;
  assert(journal.start(sink, sink.session_id()));
  assert(journal.finish(queue, sink));
  assert(sink.close());
  const auto& saved = disk.at("/orcsdr/dead_air/session_0000000123.jsonl");
  assert(saved.find("\"type\":\"session_start\"") != std::string::npos);
  assert(saved.find("\"kind\":\"manual_marker\"") != std::string::npos);
  assert(saved.find("\"type\":\"session_end\"") != std::string::npos);
  SdJournalSink next;
  assert(next.open(fs, 123));
  assert(next.session_id() == 124); // no truncation/overwriting
  assert(next.close());
  SdJournalSink bad;
  assert(bad.open(fs, 125));
  partial_write = true;
  assert(!bad.append("abcdef\n", 7));
  assert(bad.failed());
  assert(!bad.append("more\n", 5));
  assert(!bad.close());
  partial_write = false;
  g_sd_mounted = false;
  SdJournalSink absent;
  assert(!absent.open(fs, 126));
  assert(absent.failed());
  std::puts("Dead Air SD sink contract tests PASS");
}
