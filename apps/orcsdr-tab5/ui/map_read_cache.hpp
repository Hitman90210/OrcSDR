#pragma once
#include "orcmap/byte_source.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>

namespace orcsdr::maps {
// One immutable archive per attachment; storage and lifetime belong to the worker.
// No allocation or internal-DMA fallback. A missing buffer uses direct reads.
class ReadCache final : public orcmap::ByteSource {
 public:
  static constexpr size_t kBlockBytes = 4096, kSlots = 64;
  static constexpr size_t kCapacity = kBlockBytes * kSlots;
  struct Stats { uint64_t hits=0, misses=0, backing_reads=0, backing_bytes=0; };
  void Reset(orcmap::ByteSource* source=nullptr, void* buffer=nullptr, size_t capacity=0) {
    source_=source; buffer_=static_cast<uint8_t*>(buffer);
    slots_=buffer_ ? std::min(capacity/kBlockBytes,kSlots) : 0;
    for(auto& slot:slots_data_)slot={};
    stats_={};
  }
  void ResetStats() { stats_={}; }
  const Stats& stats() const { return stats_; }
  size_t Capacity() const { return slots_*kBlockBytes; }
  uint64_t Size() const override { return source_ ? source_->Size() : 0; }
  bool Valid() const override { return source_ && source_->Valid(); }
  size_t Read(uint64_t offset,void* output,size_t count) override {
    if(!Valid() || !output || offset>=Size() || !count)return 0;
    count=static_cast<size_t>(std::min<uint64_t>(count,Size()-offset));
    if(!slots_)return BackingRead(offset,output,count);
    auto* dest=static_cast<uint8_t*>(output);
    size_t done=0;
    while(done<count) {
      const uint64_t position=offset+done, base=position-position%kBlockBytes;
      const size_t index=(base/kBlockBytes)%slots_, within=position-base;
      auto& slot=slots_data_[index];
      auto* data=buffer_+index*kBlockBytes;
      if(!slot.bytes || slot.offset!=base) {
        ++stats_.misses; slot={};
        const size_t wanted=static_cast<size_t>(std::min<uint64_t>(kBlockBytes,Size()-base));
        const size_t got=BackingRead(base,data,wanted);
        if(got!=wanted) {
          // Do not retain a short/error read; a later request must retry storage.
          const size_t available=got>within ? std::min(count-done,got-within) : 0;
          if(available)std::memcpy(dest+done,data+within,available);
          return done+available;
        }
        slot.offset=base;slot.bytes=got;
      } else ++stats_.hits;
      const size_t take=std::min(count-done,slot.bytes-within);
      std::memcpy(dest+done,data+within,take);done+=take;
    }
    return done;
  }
 private:
  size_t BackingRead(uint64_t offset,void* output,size_t count) {
    ++stats_.backing_reads;
    const size_t got=source_->Read(offset,output,count);
    stats_.backing_bytes+=got;return got;
  }
  struct Slot { uint64_t offset=0;size_t bytes=0; };
  orcmap::ByteSource* source_=nullptr;
  uint8_t* buffer_=nullptr;
  size_t slots_=0;
  std::array<Slot,kSlots> slots_data_{};
  Stats stats_{};
};
} // namespace orcsdr::maps
