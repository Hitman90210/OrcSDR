#pragma once
#include "dead_air_journal.hpp"
#include "orcsdr_storage.hpp"
#include <cstddef>
#include <cstdint>

namespace orcsdr::dead_air {

// Strictly worker-owned. Never open/write/sync from the USB, RF/DSP, or UI
// callback. On partial writes this sink becomes permanently failed; retain
// the file for offline recovery instead of appending to a corrupt last line.
class SdJournalSink final : public JournalSink {
 public:
  // Picks an unused session id at or above the suggested id (up to 128 tries).
  // The caller must use session_id() when calling EventJournal::start().
  bool open(storage::FileSystem& filesystem, uint32_t suggested_id);
  bool append(const char* line, size_t length) override;
  bool sync() override;
  bool close();
  bool opened() const { return static_cast<bool>(file_); }
  bool failed() const { return failed_; }
  uint32_t session_id() const { return session_id_; }
  const char* path() const { return path_; }
 private:
  storage::File file_{};
  char path_[72]{};
  uint32_t session_id_ = 0;
  bool failed_ = false;
};

} // namespace orcsdr::dead_air
