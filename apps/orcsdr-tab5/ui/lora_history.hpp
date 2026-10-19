#pragma once
#include "lora_packet_log.hpp"
#include <cstddef>
#include <cstdint>

namespace orcsdr::lora_history {
constexpr size_t kNodeCapacity = 256;
constexpr size_t kLineMax = 768;
struct Node {
  uint32_t id = 0, first_utc = 0, last_utc = 0, packets = 0;
  uint32_t name_utc = 0, position_utc = 0, last_ms = 0;
  int32_t latitude_e7 = INT32_MAX, longitude_e7 = INT32_MAX;
  int16_t signal_tenths = INT16_MAX, snr_tenths = INT16_MAX;
  char short_name[8]{}, long_name[32]{};
};
// Newest 256 devices in memory; the append-only archive retains older history.
class Store {
 public:
  void clear();
  bool observe(const lora_log::Record& record);
  size_t count() const { return count_; }
  const Node* at(size_t index) const { return index < count_ ? &nodes_[index] : nullptr; }
  const Node* find(uint32_t id) const;
 private:
  Node nodes_[kNodeCapacity]{};
  struct Key { uint32_t sender, packet; } recent_[128]{};
  size_t count_ = 0, next_key_ = 0;
};
size_t format_line(const lora_log::Record& record, char* out, size_t size);
bool parse_line(const char* line, size_t size, lora_log::Record* out);
// Safe file component; invalid wallclock stays explicitly undated.
void day_key(uint32_t utc, char* out, size_t size);
}  // namespace orcsdr::lora_history
