#pragma once

#include <algorithm>
#include <array>
#include <vector>
#include "orcmap/feature_replay.hpp"
#include "orcmap/viewport.hpp"

namespace orcsdr::maps {

// Pipeline-owned metadata, externally owned arena. All access is under the
// existing map worker lock; no independent display or source ownership.
class FeatureCache {
 public:
  void Reset(uint8_t* arena = nullptr, size_t capacity = 0) {
    arena_ = arena; capacity_ = arena ? capacity : 0;
    count_ = used_ = 0; pending_ = false;
  }
  void Clear() { count_ = used_ = 0; pending_ = false; }
  size_t Used() const { return used_; }
  size_t Capacity() const { return capacity_; }
  size_t Count() const { return count_; }

  void Prepare(const std::vector<orcmap::TilePlacement>& visible, bool places) {
    pending_ = false;
    // Keep only tiles usable by this frame, then pack them in their existing
    // memory order so an overlapping move cannot overwrite a later source.
    size_t retained = 0;
    for (size_t i = 0; i < count_; ++i) {
      const bool present = std::any_of(visible.begin(), visible.end(),
          [&](const auto& p) { return p.tile == entries_[i].tile; });
      if (present && entries_[i].places == places) entries_[retained++] = entries_[i];
    }
    count_ = retained;
    std::sort(entries_.begin(), entries_.begin() + count_,
        [](const auto& a, const auto& b) { return a.offset < b.offset; });
    used_ = 0;
    for (size_t i = 0; i < count_; ++i) {
      if (!entries_[i].replay.Relocate(arena_ + used_, capacity_ - used_)) {
        Clear(); return;
      }
      entries_[i].offset = used_; used_ += entries_[i].replay.Size();
    }
  }
  const orcmap::FeatureReplay* Find(orcmap::TileId tile) const {
    for (size_t i = 0; i < count_; ++i)
      if (entries_[i].tile == tile) return &entries_[i].replay;
    return nullptr;
  }
  bool Begin(orcmap::TileId tile, bool places) {
    pending_ = arena_ && used_ < capacity_ && count_ < entries_.size();
    if (!pending_) return false;
    writing_ = {tile, orcmap::FeatureReplay(arena_ + used_, capacity_ - used_), used_, places};
    return true;
  }
  void Capture(const orcmap::Feature& feature) {
    if (pending_ && !writing_.replay.Append(feature)) pending_ = false;
  }
  bool Commit(bool complete) {
    const bool publish = complete && pending_;
    if (publish) {
      used_ += writing_.replay.Size(); entries_[count_++] = writing_;
    }
    pending_ = false;
    return publish;
  }
 private:
  struct Entry {
    orcmap::TileId tile{};
    orcmap::FeatureReplay replay{nullptr, 0};
    size_t offset = 0;
    bool places = false;
  };
  std::array<Entry, 64> entries_{};
  Entry writing_{};
  uint8_t* arena_ = nullptr;
  size_t capacity_ = 0, count_ = 0, used_ = 0;
  bool pending_ = false;
};

}  // namespace orcsdr::maps
