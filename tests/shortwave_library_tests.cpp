#include "shortwave_library.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

[[noreturn]] void fail(const char* expression, int line) {
  std::fprintf(stderr, "FAIL line=%d check=%s\n", line, expression);
  std::exit(1);
}

#define CHECK(expression) \
  do { if (!(expression)) fail(#expression, __LINE__); } while (false)

}  // namespace

int main() {
  using namespace orcsdr::shortwave;
  LogEntry entry{};
  entry.timestamp_utc = 1789440123;
  entry.local_offset_minutes = -7 * 60;
  entry.frequency_hz = 9800000;
  entry.bandwidth_hz = 6000;
  entry.signal_dbfs = -42.5f;
  std::strcpy(entry.mode, "AM");
  std::strcpy(entry.station, "Example, International");
  std::strcpy(entry.language, "English");
  std::strcpy(entry.antenna, "MLA-30+");
  std::strcpy(entry.notes, "Voice \"clear\"\nthen music");

  char csv[2048]{};
  CHECK(encode_log_csv(entry, csv, sizeof(csv)));
  LogEntry decoded{};
  CHECK(decode_log_csv(csv, &decoded));
  CHECK(decoded.timestamp_utc == entry.timestamp_utc);
  CHECK(decoded.frequency_hz == entry.frequency_hz);
  CHECK(std::strcmp(decoded.station, entry.station) == 0);
  CHECK(std::strcmp(decoded.notes, entry.notes) == 0);

  char adif[2048]{};
  CHECK(encode_adif(entry, adif, sizeof(adif)));
  CHECK(std::strstr(adif, "<SWL:1>Y") != nullptr);
  CHECK(std::strstr(adif, "<FREQ:6>9.8000") != nullptr);
  CHECK(std::strstr(adif, "<MODE:2>AM") != nullptr);
  CHECK(std::strstr(adif, "<EOR>") != nullptr);
  CHECK(std::strstr(adif, "MLA-30+") != nullptr);

  Memory memory{};
  memory.frequency_hz = 9800000;
  memory.bandwidth_hz = 6000;
  memory.favorite = true;
  std::strcpy(memory.mode, "AM");
  std::strcpy(memory.station, "Example, International");
  std::strcpy(memory.notes, "Evening test");
  CHECK(encode_memory_csv(memory, csv, sizeof(csv)));
  Memory decoded_memory{};
  CHECK(decode_memory_csv(csv, &decoded_memory));
  CHECK(decoded_memory.favorite);
  CHECK(std::strcmp(decoded_memory.station, memory.station) == 0);

  std::puts("shortwave_library_tests: PASS");
  return 0;
}
