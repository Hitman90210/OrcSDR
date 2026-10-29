#pragma once

#include <cstddef>
#include <cstdint>

namespace orcsdr::storage { class File; class FileSystem; }

namespace orcsdr::lora_log {

struct Record {
  char text[112]{};
  uint32_t sender = 0;
  uint32_t destination = 0;
  uint32_t packet_id = 0;
  uint32_t received_ms = 0;
  uint32_t received_utc = 0;
  int32_t latitude_e7 = INT32_MAX;
  int32_t longitude_e7 = INT32_MAX;
  int16_t snr_tenths = INT16_MAX;
  int16_t signal_tenths = INT16_MAX;
  uint16_t port = 0;
  uint32_t frequency_hz = 0;
  bool encrypted = false;
  bool from_host = false;  // authenticated host injection, not RF identity evidence
  bool history_observed = false;  // queue bookkeeping, never serialized
  char short_name[8]{};
  char long_name[32]{};
};

size_t format_csv(char* output, size_t output_size, const Record& record);
struct AppendCheckpoint {
  size_t journal_begin=0, csv_begin=0, journal_end=0, csv_end=0;
  bool valid=false;
};
struct AppendResult {
  const char* stage=nullptr;
  int error=0;
  explicit operator bool() const { return stage==nullptr; }
};
// Keep the record/checkpoint until success; reopen failed files before retrying.
AppendResult append_record(storage::File& journal, storage::File& csv,
                           const Record& record, AppendCheckpoint& checkpoint,
                           char* buffer, size_t capacity);
bool export_snapshot(storage::FileSystem& filesystem, const Record* records, size_t count,
                     uint16_t* sequence, char* path, size_t path_size);
bool self_check();

}  // namespace orcsdr::lora_log
