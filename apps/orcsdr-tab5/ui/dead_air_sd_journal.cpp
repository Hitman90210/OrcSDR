#include "dead_air_sd_journal.hpp"
#include <cstdio>
#include <cstring>

namespace orcsdr::dead_air {

bool SdJournalSink::open(storage::FileSystem& fs, uint32_t suggested_id) {
  if (opened() || suggested_id == 0) return false;
  failed_ = false;
  session_id_ = 0;
  path_[0] = '\0';
  if (!storage::mounted() || !(fs.exists("/orcsdr") || fs.mkdir("/orcsdr")) || !fs.mkdir("/orcsdr/dead_air")) {
    failed_ = true;
    return false;
  }
  for (uint32_t i = 0; i < 128; ++i) {
    const uint32_t id = suggested_id + i;
    if (id == 0) continue; // reject rollover as a valid id
    char candidate[sizeof(path_)];
    const int n = std::snprintf(candidate, sizeof(candidate),
                                "/orcsdr/dead_air/session_%010lu.jsonl",
                                static_cast<unsigned long>(id));
    if (n <= 0 || static_cast<size_t>(n) >= sizeof(candidate)) {
      failed_ = true;
      return false;
    }
    if (fs.exists(candidate)) continue; // never truncate an earlier session
    file_ = fs.open(candidate, FILE_APPEND);
    if (!file_) {
      failed_ = true;
      return false;
    }
    std::memcpy(path_, candidate, static_cast<size_t>(n) + 1);
    session_id_ = id;
    return true;
  }
  failed_ = true;
  return false;
}

bool SdJournalSink::append(const char* bytes, size_t len) {
  if (!opened() || failed_ || !bytes || len == 0) return false;
  if (file_.write(reinterpret_cast<const uint8_t*>(bytes), len) != len) {
    failed_ = true;
    return false;
  }
  return true;
}
bool SdJournalSink::sync() {
  if (!opened() || failed_ || !file_.sync()) {
    failed_ = true;
    return false;
  }
  return true;
}
bool SdJournalSink::close() {
  if (!opened()) return false;
  const bool synced = !failed_ && sync();
  const bool closed = file_.close();
  return synced && closed;
}
} // namespace orcsdr::dead_air
