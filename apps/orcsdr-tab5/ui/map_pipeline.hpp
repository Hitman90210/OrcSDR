#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include "map_world_compat.hpp"
#include "map_feature_cache.hpp"
#include "orcmap/byte_source.hpp"
#include "orcmap/mvt_stream.hpp"
#include "orcmap/pmtiles.hpp"
#include "orcmap/render_target.hpp"
#include "orcmap/viewport.hpp"
#include "orcmap/location_picker.hpp"
#include "orcmap/labels.hpp"
#include "orcmap/renderer.hpp"

namespace orcsdr::maps {

struct WorkHooks {
  uint64_t (*now_us)() = nullptr;
  bool (*service)(void*) = nullptr; // false cancels; device worker yields here
  void* context = nullptr;
};

struct RenderStats {
  uint32_t tiles = 0, missing = 0, failed = 0, features = 0;
  uint32_t reads = 0;
  uint64_t bytes = 0, io_us = 0, lookup_us = 0, stream_us = 0, render_us = 0, total_us = 0;
  bool cancelled = false, out_of_memory = false;
  orcmap::LabelStats labels{};
  uint64_t label_us = 0;
  size_t deferred_peak_bytes = 0, deferred_retained_bytes = 0;
  uint32_t deferred_capacity_failures = 0;
  uint32_t replay_hits = 0, replay_fallbacks = 0;
  size_t replay_bytes = 0, replay_capacity = 0;
};

// One source/reader/scratch per active archive. No display, radio, or network
// ownership lives here; the device adapter supplies an off-screen target.
class Pipeline {
 public:
  explicit Pipeline(orcmap::ByteSource& source, WorkHooks hooks = {});
  bool open(std::string_view profile);
  // Caller owns lifetime/PSRAM placement and must use the map worker lock.
  void set_feature_cache(FeatureCache* cache) { feature_cache_ = cache; if (cache) cache->Clear(); }
  // Optional labels use caller-owned storage (PSRAM on P4) and a real font target.
  bool render(const orcmap::Viewport& viewport, orcmap::RenderTarget& target,
              orcmap::LabelLayout* labels = nullptr, orcmap::LabelTextTarget* text = nullptr,
              int label_zoom = -1); // User zoom; source viewport may be coarser. -1 uses viewport.zoom.
  const orcmap::PmTilesHeader& header() const { return reader_.Header(); }
  const RenderStats& stats() const { return stats_; }

 private:
  class Source final : public orcmap::ByteSource {
   public:
    Source(orcmap::ByteSource& source, RenderStats& stats, WorkHooks& hooks)
        : source_(source), stats_(stats), hooks_(hooks) {}
    size_t Read(uint64_t offset, void* output, size_t size) override;
    uint64_t Size() const override { return source_.Size(); }
    bool Valid() const override { return source_.Valid(); }
   private:
    orcmap::ByteSource& source_;
    RenderStats& stats_;
    WorkHooks& hooks_;
  };
  struct Context;
  bool open_impl(std::string_view profile);
  bool render_impl(const orcmap::Viewport&,orcmap::RenderTarget&,
                   orcmap::LabelLayout*,orcmap::LabelTextTarget*,int);
  static bool feature(const orcmap::Feature&, void*);
  uint64_t now() const { return hooks_.now_us ? hooks_.now_us() : 0; }
  WorkHooks hooks_;
  RenderStats stats_{};
  Source source_;
  orcmap::PmTilesReader reader_;
  orcmap::MvtStreamScratch scratch_;
  orcmap::OrderedRenderScratch render_scratch_;
  FeatureCache* feature_cache_ = nullptr;
  bool ready_ = false;
  std::string_view profile_; // points only to a supported static literal
};

// Application geometry goes through OrcMaps too; the coordinates remain E7 at
// the boundary. Invalid dimensions/coordinates fail rather than plotting zero.
bool regional_view(int32_t latitude_e7, int32_t longitude_e7, double range_nm,
                   int width, int height, orcmap::Viewport* output);
bool project(const orcmap::Viewport&, int32_t latitude_e7, int32_t longitude_e7,
             int* x, int* y);
bool begin_location_picker(orcmap::LocationPicker& picker,int width,int height,
                           int32_t latitude_e7,int32_t longitude_e7);
bool world_overview_view(const orcmap::Viewport& requested,uint8_t minimum,uint8_t maximum,
                         orcmap::Viewport* output);
// Select stored tile detail while retaining the requested Mercator camera.
bool pack_source_view(const orcmap::Viewport& requested,uint8_t minimum,uint8_t maximum,
                      orcmap::Viewport* output);

} // namespace orcsdr::maps
