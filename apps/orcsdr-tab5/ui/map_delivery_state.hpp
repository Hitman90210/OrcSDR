#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace orcsdr::maps::delivery {
constexpr size_t kMaximumStateBytes=8192,kMaximumOwnedPacks=16;
enum class Error {none,input,limit,storage,corrupt,rollback,allocation};
enum class Read {ok,missing,error};
struct OwnedPack {std::string product,manifest;};
struct PendingInstall {std::string product,manifest;bool select=false;};
struct State {
  uint64_t accepted_revision=0;
  std::string catalog_sha256,selected_product;
  std::vector<OwnedPack> owned;
  PendingInstall pending;
};
// One serialized installer owns this store. Implementations must distinguish
// missing data from read errors and atomically replace a single blob.
struct Store {
  virtual ~Store()=default;
  virtual Read read(std::string*)=0;
  virtual bool write(std::string_view)=0;
};
Error load(Store&,State*) noexcept;
// Preserves corrupt records, rejects rollback/equivocation, verifies readback.
// The caller must authenticate the catalog before advancing revision/hash.
Error save(Store&,const State&) noexcept;
// Persist this intent before activation so a restart can finish ownership even
// without a network/catalog fetch. Existing ownership remains unchanged.
Error plan_install(State&,std::string_view product,std::string_view manifest,bool select) noexcept;
// Explicit catalog product identity, never a display-name or region heuristic.
// Requires matching intent. Call after activation; persist before finalization.
Error record_install(State&,std::string_view product,std::string_view manifest,bool select) noexcept;
} // namespace orcsdr::maps::delivery
