#include "shortwave_library.hpp"

#include <cstdio>
#include <cstring>

namespace orcsdr::shortwave {
namespace {

void set_error(char* error, size_t capacity, const char* message) {
  if (error && capacity) std::snprintf(error, capacity, "%s", message);
}

bool read_record(File& file, char* output, size_t capacity) {
  if (!output || capacity < 2) return false;
  size_t used = 0;
  bool quoted = false;
  while (file.available()) {
    char value = '\0';
    if (file.read(&value, 1) != 1) break;
    if (used + 1 >= capacity) return false;
    output[used++] = value;
    if (value == '\"') {
      if (!quoted) {
        quoted = true;
      } else if (file.available()) {
        char next = '\0';
        if (file.read(&next, 1) != 1) return false;
        if (used + 1 >= capacity) return false;
        output[used++] = next;
        if (next != '\"') {
          quoted = false;
          if (next == '\n') break;
        }
      } else {
        quoted = false;
      }
    } else if (value == '\n' && !quoted) {
      break;
    }
  }
  while (used && (output[used - 1] == '\r' || output[used - 1] == '\n')) --used;
  output[used] = '\0';
  return used != 0;
}

template <typename Table, typename Record>
bool load_table(storage::FileSystem& fs, const char* path, Table* table,
                bool (*decode)(const char*, Record*), size_t* invalid_rows,
                char* error, size_t error_capacity) {
  table->clear();
  if (!fs.exists(path)) return true;
  File file = fs.open(path, FILE_READ);
  if (!file) {
    set_error(error, error_capacity, "cannot open Shortwave library");
    return false;
  }
  char record[2048];
  while (read_record(file, record, sizeof(record))) {
    if (record[0] == '#') continue;
    Record value{};
    if (!decode(record, &value) || table->upsert(value) != RecordResult::ok)
      ++*invalid_rows;
  }
  file.close();
  return true;
}

template <>
bool load_table<LogTable, LogEntry>(storage::FileSystem& fs, const char* path,
                                    LogTable* table,
                                    bool (*decode)(const char*, LogEntry*),
                                    size_t* invalid_rows, char* error,
                                    size_t error_capacity) {
  table->clear();
  if (!fs.exists(path)) return true;
  File file = fs.open(path, FILE_READ);
  if (!file) {
    set_error(error, error_capacity, "cannot open Shortwave logbook");
    return false;
  }
  char record[2048];
  while (read_record(file, record, sizeof(record))) {
    if (record[0] == '#') continue;
    LogEntry value{};
    if (!decode(record, &value) || table->append(value) != RecordResult::ok)
      ++*invalid_rows;
  }
  file.close();
  return true;
}

template <typename Table, typename Record>
bool write_table(storage::FileSystem& fs, const char* path, const char* header,
                 const Table& table,
                 bool (*encode)(const Record&, char*, size_t),
                 bool (*decode)(const char*, Record*), char* error,
                 size_t error_capacity) {
  char temporary[160], backup[160];
  std::snprintf(temporary, sizeof(temporary), "%s.part", path);
  std::snprintf(backup, sizeof(backup), "%s.bak", path);
  fs.mkdir("/OrcSDR");
  fs.mkdir(kLibraryRoot);
  fs.remove(temporary);
  File file = fs.open(temporary, FILE_WRITE, true);
  if (!file) {
    set_error(error, error_capacity, "cannot create Shortwave library");
    return false;
  }
  file.print(header);
  char record[2048];
  for (size_t i = 0; i < table.size(); ++i) {
    const Record* value = table.at(i);
    if (!value || !encode(*value, record, sizeof(record)) ||
        file.print(record) == 0 || file.print("\r\n") == 0) {
      file.close();
      fs.remove(temporary);
      set_error(error, error_capacity, "cannot write Shortwave library");
      return false;
    }
  }
  file.flush();
  file.close();

  File verify = fs.open(temporary, FILE_READ);
  if (!verify) {
    fs.remove(temporary);
    set_error(error, error_capacity, "cannot verify Shortwave library");
    return false;
  }
  while (read_record(verify, record, sizeof(record))) {
    if (record[0] == '#') continue;
    Record value{};
    if (!decode(record, &value)) {
      verify.close();
      fs.remove(temporary);
      set_error(error, error_capacity, "Shortwave library verification failed");
      return false;
    }
  }
  verify.close();

  fs.remove(backup);
  const bool had_target = fs.exists(path);
  if (had_target && !fs.rename(path, backup)) {
    fs.remove(temporary);
    set_error(error, error_capacity, "cannot back up Shortwave library");
    return false;
  }
  if (!fs.rename(temporary, path)) {
    if (had_target) fs.rename(backup, path);
    fs.remove(temporary);
    set_error(error, error_capacity, "cannot replace Shortwave library");
    return false;
  }
  return true;
}

bool write_exports(storage::FileSystem& fs, const LogTable& logs, char* error,
                   size_t error_capacity) {
  fs.mkdir("/OrcSDR");
  fs.mkdir(kExportRoot);
  File csv = fs.open("/OrcSDR/exports/shortwave-logbook.csv", FILE_WRITE, true);
  File adi = fs.open("/OrcSDR/exports/shortwave-logbook.adi", FILE_WRITE, true);
  if (!csv || !adi) {
    csv.close(); adi.close();
    set_error(error, error_capacity, "cannot create Shortwave exports");
    return false;
  }
  csv.print("# OrcSDR Shortwave logbook CSV v1\r\n");
  char record[2048];
  for (size_t i = 0; i < logs.size(); ++i) {
    const LogEntry* entry = logs.at(i);
    if (!entry || !encode_log_csv(*entry, record, sizeof(record)) ||
        !csv.print(record) || !csv.print("\r\n") ||
        !encode_adif(*entry, record, sizeof(record)) || !adi.print(record)) {
      csv.close(); adi.close();
      set_error(error, error_capacity, "cannot write Shortwave exports");
      return false;
    }
  }
  csv.flush(); adi.flush(); csv.close(); adi.close();
  return true;
}

}  // namespace

bool load_library(storage::FileSystem& fs, LibraryState* state, char* error,
                  size_t error_capacity) {
  if (!state) return false;
  state->invalid_rows = 0;
  if (!load_table(fs, kMemoriesPath, &state->memories, decode_memory_csv,
                  &state->invalid_rows, error, error_capacity) ||
      !load_table(fs, kLogbookPath, &state->logs, decode_log_csv,
                  &state->invalid_rows, error, error_capacity)) {
    state->status = StorageStatus::read_failed;
    return false;
  }
  state->status = StorageStatus::ready;
  return true;
}

bool save_memories(storage::FileSystem& fs, const MemoryTable& memories,
                   char* error, size_t error_capacity) {
  return write_table(fs, kMemoriesPath, "# OrcSDR Shortwave memories CSV v1\r\n",
                     memories, encode_memory_csv, decode_memory_csv, error,
                     error_capacity);
}

bool save_logs(storage::FileSystem& fs, const LogTable& logs, char* error,
               size_t error_capacity) {
  return write_table(fs, kLogbookPath, "# OrcSDR Shortwave logbook CSV v1\r\n",
                     logs, encode_log_csv, decode_log_csv, error, error_capacity);
}

bool export_logs(storage::FileSystem& fs, const LogTable& logs, char* error,
                 size_t error_capacity) {
  return write_exports(fs, logs, error, error_capacity);
}

}  // namespace orcsdr::shortwave
