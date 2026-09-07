#include "pocsag_store.hpp"

#include <cstring>

namespace orcsdr::pocsag_store {
namespace {

void copy_field(char* dest, size_t dest_size, const char* src) {
  if (!dest || dest_size == 0) return;
  if (!src) {
    dest[0] = '\0';
    return;
  }
  std::strncpy(dest, src, dest_size - 1);
  dest[dest_size - 1] = '\0';
}

}  // namespace

void Table::reset() {
  for (auto& identity : identities_) identity = Identity{};
  count_ = 0;
}

int Table::find(uint32_t capcode) const {
  for (size_t i = 0; i < count_; ++i)
    if (identities_[i].capcode == capcode) return static_cast<int>(i);
  return -1;
}

const Identity* Table::at(size_t index) const {
  return index < count_ ? &identities_[index] : nullptr;
}

int Table::record_hit(uint32_t capcode, uint64_t now_ms) {
  const int existing = find(capcode);
  if (existing >= 0) {
    Identity& identity = identities_[static_cast<size_t>(existing)];
    identity.last_seen_ms = now_ms;
    ++identity.hit_count;
    return existing;
  }

  size_t slot;
  if (count_ < kMaxIdentities) {
    slot = count_++;
  } else {
    // Table full: evict the least-recently-seen unwatched record. If every
    // record is watched, refuse rather than silently dropping a watch.
    int oldest = -1;
    uint64_t oldest_seen = UINT64_MAX;
    for (size_t i = 0; i < kMaxIdentities; ++i) {
      if (identities_[i].watched) continue;
      if (identities_[i].last_seen_ms < oldest_seen) {
        oldest_seen = identities_[i].last_seen_ms;
        oldest = static_cast<int>(i);
      }
    }
    if (oldest < 0) return -1;
    slot = static_cast<size_t>(oldest);
  }

  identities_[slot] = Identity{};
  identities_[slot].capcode = capcode;
  identities_[slot].first_seen_ms = now_ms;
  identities_[slot].last_seen_ms = now_ms;
  identities_[slot].hit_count = 1;
  return static_cast<int>(slot);
}

bool Table::set_alias(uint32_t capcode, const char* alias) {
  const int index = find(capcode);
  if (index < 0) return false;
  copy_field(identities_[static_cast<size_t>(index)].alias, kAliasLen, alias);
  return true;
}

bool Table::set_group(uint32_t capcode, const char* group) {
  const int index = find(capcode);
  if (index < 0) return false;
  copy_field(identities_[static_cast<size_t>(index)].group, kGroupLen, group);
  return true;
}

bool Table::set_watched(uint32_t capcode, bool watched) {
  const int index = find(capcode);
  if (index < 0) return false;
  identities_[static_cast<size_t>(index)].watched = watched;
  return true;
}

bool Table::set_muted(uint32_t capcode, bool muted) {
  const int index = find(capcode);
  if (index < 0) return false;
  identities_[static_cast<size_t>(index)].muted = muted;
  return true;
}

bool Table::set_notes(uint32_t capcode, const char* notes) {
  const int index = find(capcode);
  if (index < 0) return false;
  copy_field(identities_[static_cast<size_t>(index)].notes, kNotesLen, notes);
  return true;
}

bool Table::restore(const Identity& identity) {
  if (count_ >= kMaxIdentities || find(identity.capcode) >= 0) return false;
  identities_[count_++] = identity;
  return true;
}

bool Table::self_check() {
  Table table;
  if (table.record_hit(111, 1000) != 0) return false;
  if (table.record_hit(111, 2000) != 0) return false;  // same slot, bumps hits
  const Identity* first = table.at(0);
  if (!first || first->hit_count != 2 || first->last_seen_ms != 2000 ||
      first->first_seen_ms != 1000)
    return false;

  if (!table.set_alias(111, "FACILITIES")) return false;
  if (!table.set_group(111, "OPS")) return false;
  if (!table.set_watched(111, true)) return false;
  if (!table.set_notes(111, "Building maintenance pager")) return false;
  first = table.at(0);
  if (std::strcmp(first->alias, "FACILITIES") != 0) return false;
  if (std::strcmp(first->group, "OPS") != 0) return false;
  if (!first->watched) return false;

  // Unknown capcode edits must fail cleanly, not create a record.
  if (table.set_alias(999, "GHOST")) return false;
  if (table.count() != 1) return false;

  // Fill to capacity with distinct, unwatched capcodes.
  for (uint32_t i = 0; i < kMaxIdentities - 1; ++i) {
    if (table.record_hit(1000 + i, 3000 + i) < 0) return false;
  }
  if (table.count() != kMaxIdentities) return false;

  // Table is full; capcode 111 is watched so it must survive eviction, and
  // the least-recently-seen unwatched record (capcode 1000, seen at 3000)
  // must be the one evicted for a brand-new capcode.
  const int new_index = table.record_hit(5555, 99999);
  if (new_index < 0) return false;
  if (table.find(111) < 0) return false;   // watched record survived
  if (table.find(1000) >= 0) return false;  // oldest unwatched was evicted
  if (table.find(5555) < 0) return false;

  // restore() must reject duplicates and respect the capacity bound.
  Table restore_table;
  Identity saved{};
  saved.capcode = 42;
  if (!restore_table.restore(saved)) return false;
  if (restore_table.restore(saved)) return false;  // duplicate capcode

  return true;
}

}  // namespace orcsdr::pocsag_store
