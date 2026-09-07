#pragma once

#include <cstddef>
#include <cstdint>

// CAPCODE identity table: pure in-RAM logic, no FreeRTOS/SD dependency here
// (matching the pocsag_decoder_core precedent), so it is host-testable.
// SD persistence (orcsdr::pocsag_store::load/save, in pocsag_store_io.cpp)
// is a separate, non-host-tested layer that only translates this table
// to/from /orcsdr/pocsag/identities.cfg using the same atomic .part/.bak
// write pattern as the existing P25 profile store.
namespace orcsdr::pocsag_store {

constexpr size_t kMaxIdentities = 32;
constexpr size_t kAliasLen = 20;
constexpr size_t kGroupLen = 12;
constexpr size_t kNotesLen = 32;

struct Identity {
  uint32_t capcode = 0;
  char alias[kAliasLen] = {};
  char group[kGroupLen] = {};
  bool watched = false;
  bool muted = false;
  char notes[kNotesLen] = {};
  uint64_t first_seen_ms = 0;
  uint64_t last_seen_ms = 0;
  uint32_t hit_count = 0;
};

class Table {
 public:
  void reset();

  // Records a decoded message's capcode: creates a bounded identity record
  // if this capcode has not been seen before (evicting the least-recently-
  // seen unwatched record if the table is full; watched records are never
  // evicted), and always bumps hit_count/last_seen. Returns the identity's
  // index, or -1 only if the table is full of watched records.
  int record_hit(uint32_t capcode, uint64_t now_ms);

  int find(uint32_t capcode) const;
  size_t count() const { return count_; }
  const Identity* at(size_t index) const;

  bool set_alias(uint32_t capcode, const char* alias);
  bool set_group(uint32_t capcode, const char* group);
  bool set_watched(uint32_t capcode, bool watched);
  bool set_muted(uint32_t capcode, bool muted);
  bool set_notes(uint32_t capcode, const char* notes);

  // Direct restore from a persisted record (SD load), bypassing the
  // hit-tracking semantics of record_hit. Returns false if the table is
  // full or the capcode already exists.
  bool restore(const Identity& identity);

  static bool self_check();

 private:
  Identity identities_[kMaxIdentities]{};
  size_t count_ = 0;
};

}  // namespace orcsdr::pocsag_store
