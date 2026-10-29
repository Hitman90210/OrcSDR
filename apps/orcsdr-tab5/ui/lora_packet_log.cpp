#include "lora_packet_log.hpp"
#include "orcsdr_storage.hpp"
#include "lora_history.hpp"

#include <cerrno>
#include <cstdio>
#include <cstring>

namespace orcsdr::lora_log {
namespace {
constexpr char kCsvHeader[] = "uptime_ms,wallclock_valid,wallclock_source,received_utc,frequency_hz,from,to,packet_id,port,snr_tenths,signal_tenths,latitude_e7,longitude_e7,text,short_name,long_name,encrypted,from_host\n";
}

AppendResult append_record(storage::File& journal, storage::File& csv,
                          const Record& record, AppendCheckpoint& checkpoint,
                          char* buffer, size_t capacity) {
  if(!journal || !csv) return {"append_unavailable",0};
  const size_t journal_bytes=lora_history::format_line(record,buffer,capacity);
  if(!journal_bytes)return {"journal_format",0};
  const size_t csv_bytes=format_csv(buffer,capacity,record);
  if(!csv_bytes)return {"csv_format",0};
  errno=0;
  const size_t journal_size=journal.size();
  if(errno)return {"journal_size",errno};
  errno=0;
  const size_t csv_size=csv.size();
  if(errno)return {"csv_size",errno};
  if(checkpoint.valid) {
    // Only this record's uncommitted suffix belongs to this writer.
    if(journal_size<checkpoint.journal_begin || journal_size>checkpoint.journal_end ||
       csv_size<checkpoint.csv_begin || csv_size>checkpoint.csv_end)
      return {"rollback_bounds",EIO};
    errno=0;
    if(!journal.truncate(checkpoint.journal_begin))return {"journal_rollback",errno};
    errno=0;
    if(!csv.truncate(checkpoint.csv_begin))return {"csv_rollback",errno};
  } else {
    const size_t header_bytes=csv_size==0?sizeof(kCsvHeader)-1:0;
    if(journal_bytes>SIZE_MAX-journal_size || csv_bytes>SIZE_MAX-header_bytes ||
       csv_bytes+header_bytes>SIZE_MAX-csv_size)return {"checkpoint_overflow",EOVERFLOW};
    checkpoint={journal_size,csv_size,journal_size+journal_bytes,
                csv_size+header_bytes+csv_bytes,true};
  }
  errno=0;
  if(!journal.seek(checkpoint.journal_begin) || !csv.seek(checkpoint.csv_begin))
    return {"append_seek",errno};
  lora_history::format_line(record,buffer,capacity);
  errno=0;
  if(journal.write(reinterpret_cast<const uint8_t*>(buffer),journal_bytes)!=journal_bytes)
    return {"journal_write",errno};
  if(checkpoint.csv_begin==0) {
    errno=0;
    if(csv.print(kCsvHeader)!=sizeof(kCsvHeader)-1)return {"csv_header",errno};
  }
  format_csv(buffer,capacity,record);
  errno=0;
  if(csv.write(reinterpret_cast<const uint8_t*>(buffer),csv_bytes)!=csv_bytes)
    return {"csv_write",errno};
  errno=0;
  if(!journal.sync())return {"journal_sync",errno};
  errno=0;
  if(!csv.sync())return {"csv_sync",errno};
  checkpoint={};
  return {};
}

size_t format_csv(char* output, size_t output_size, const Record& record) {
  if (output == nullptr || output_size < 4) return 0;
  char destination[16];
  if (record.destination == UINT32_MAX)
    snprintf(destination, sizeof(destination), "%s", "broadcast");
  else
    snprintf(destination, sizeof(destination), "!%08lx",
             static_cast<unsigned long>(record.destination));
  const int used = snprintf(
      output, output_size, "%lu,%u,%s,%lu,%lu,!%08lx,%s,%08lx,%u,%d,%d,%ld,%ld,\"",
      static_cast<unsigned long>(record.received_ms),
      record.received_utc ? 1u : 0u,
      record.received_utc ? "rtc" : "none",
      static_cast<unsigned long>(record.received_utc),
      static_cast<unsigned long>(record.frequency_hz),
      static_cast<unsigned long>(record.sender), destination,
      static_cast<unsigned long>(record.packet_id), static_cast<unsigned>(record.port),
      static_cast<int>(record.snr_tenths), static_cast<int>(record.signal_tenths),
      static_cast<long>(record.latitude_e7), static_cast<long>(record.longitude_e7));
  if (used < 0 || static_cast<size_t>(used) + 3 > output_size) return 0;
  size_t position = static_cast<size_t>(used);
  auto append_text = [&](const char* value) {
    // RF text is untrusted: quoted CSV cells can still execute formulas.
    if (*value && strchr("=+-@\t\r", *value)) {
      if (position + 1 >= output_size) return false;
      output[position++] = '\'';
    }
    for (const char* p = value; *p; ++p) {
      if (position + 4 >= output_size) return false;
      if (*p == '"') output[position++] = '"';
      output[position++] = *p;
    }
    output[position++] = '"'; return true;
  };
  if (!append_text(record.text)) return 0;
  auto append = [&](const char* value) {
    if (position + 3 >= output_size) return false;
    output[position++] = ','; output[position++] = '"';
    return append_text(value);
  };
  if (!append(record.short_name) || !append(record.long_name)) return 0;
  const int tail = snprintf(output + position, output_size - position, ",%u,%u\n",
                            unsigned(record.encrypted), unsigned(record.from_host));
  if (tail < 0 || static_cast<size_t>(tail) >= output_size - position) return 0;
  position += static_cast<size_t>(tail);
  return position;
}

bool export_snapshot(storage::FileSystem& filesystem, const Record* records, size_t count,
                     uint16_t* sequence, char* path, size_t path_size) {
  if (records == nullptr || count == 0 || sequence == nullptr || path == nullptr ||
      path_size < 24) return false;
  if (!filesystem.exists("/orcsdr") && !filesystem.mkdir("/orcsdr")) return false;
  do {
    snprintf(path, path_size, "/orcsdr/lora_%03u.csv", ++*sequence);
  } while (filesystem.exists(path));
  storage::File file = filesystem.open(path, FILE_WRITE, true);
  if (!file) return false;
  file.print("uptime_ms,wallclock_valid,wallclock_source,received_utc,frequency_hz,from,to,packet_id,port,snr_tenths,"
             "signal_tenths,latitude_e7,longitude_e7,text,short_name,long_name,encrypted,from_host\n");
  char line[512];
  for (size_t i = count; i > 0; --i) {
    const size_t bytes = format_csv(line, sizeof(line), records[i - 1]);
    if (bytes == 0 || file.write(reinterpret_cast<const uint8_t*>(line), bytes) != bytes) {
      file.close();
      return false;
    }
  }
  file.flush();
  file.close();
  return true;
}

bool self_check() {
  Record record{};
  record.sender = 0x435baa2c;
  record.destination = UINT32_MAX;
  record.packet_id = 7;
  record.received_ms = 42;
  record.received_utc = 1789000000;
  record.frequency_hz = 906875000;
  snprintf(record.text, sizeof(record.text), "%s", "test \"one\"");
  char line[256];
  const size_t bytes = format_csv(line, sizeof(line), record);
  const bool content_ok = bytes > 0 &&
                          strstr(line, "42,1,rtc,1789000000,906875000") != nullptr &&
                          strstr(line, "!435baa2c,broadcast,00000007") != nullptr &&
                          strstr(line, "\"test \"\"one\"\"\"") != nullptr;
  record.text[0] = '\0';
  const size_t empty_bytes = format_csv(line, sizeof(line), record);
  return content_ok && empty_bytes > 0 && format_csv(line, empty_bytes, record) == 0;
}

}  // namespace orcsdr::lora_log
