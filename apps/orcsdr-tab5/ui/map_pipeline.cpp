#include "map_pipeline.hpp"
#include "orcmap/experimental/mvt_classify.hpp"
#include "orcmap/renderer.hpp"
#include "orcmap/style.hpp"
#include <algorithm>
#include <cmath>
#include <vector>
#include <new>
#ifndef __cpp_exceptions
#error "OrcMaps allocation recovery requires C++ exception support"
#endif

namespace orcsdr::maps {
namespace {
bool coordinates(int32_t lat, int32_t lon) {
  return lat >= -900000000 && lat <= 900000000 && lon >= -1800000000 && lon <= 1800000000;
}
}

Pipeline::Pipeline(orcmap::ByteSource& source, WorkHooks hooks)
    : hooks_(hooks), source_(source, stats_, hooks_), reader_(&source_, 1) {}

size_t Pipeline::Source::Read(uint64_t offset, void* output, size_t size) {
  const uint64_t start = hooks_.now_us ? hooks_.now_us() : 0;
  const size_t bytes = source_.Read(offset, output, size);
  ++stats_.reads;
  stats_.bytes += bytes;
  if (hooks_.now_us) stats_.io_us += hooks_.now_us() - start;
  return bytes;
}

bool Pipeline::open(std::string_view profile) {
  try {return open_impl(profile);}
  catch(const std::bad_alloc&) {ready_=false;stats_.out_of_memory=true;++stats_.failed;return false;}
}
bool Pipeline::open_impl(std::string_view profile) {
  stats_={};
  if (feature_cache_) feature_cache_->Clear(); // Archive/profile identity changed.
  ready_ = false;
  if (profile == "openmaptiles-3.16") profile_ = "openmaptiles-3.16";
  else if (profile == "orcmaps-overview-1") profile_ = "orcmaps-overview-1";
  else if (profile == "protomaps-v4") profile_ = "protomaps-v4";
  else return false;
  if (!reader_.Open() || reader_.Header().tile_type != orcmap::TileType::kMvt) return false;
  // Scratch is reused, including the inflater and bounded one-feature buffer.
  if (!orcmap::ReserveMvtStreamScratch(&scratch_, 48 * 1024)) return false;
  ready_ = true;
  return true;
}

struct Pipeline::Context {
  Pipeline& pipeline;
  const orcmap::Viewport& viewport;
  orcmap::RenderTarget& target;
  const std::vector<orcmap::TilePlacement>& placements;
  orcmap::TileId tile;
  uint8_t label_zoom;
  orcmap::LabelLayout* labels;
  orcmap::OrderedRenderScratch& scratch;
  bool replaying = false;
};

bool Pipeline::feature(const orcmap::Feature& decoded, void* opaque) {
  auto& context = *static_cast<Context*>(opaque);
  auto& pipeline = context.pipeline;
  if (pipeline.hooks_.service && !pipeline.hooks_.service(pipeline.hooks_.context)) {
    pipeline.stats_.cancelled = true;
    return false;
  }
  ++pipeline.stats_.features;
  // The streaming callback owns this reused feature until it returns. The
  // classifier is shared by streaming and batch engine consumers.
  auto& feature = const_cast<orcmap::Feature&>(decoded);
  if (!context.replaying) {
    feature.kind_assigned = orcmap::experimental::TryClassifyFeatureForProfile(
        pipeline.profile_, feature, &feature.kind);
    if (pipeline.feature_cache_) pipeline.feature_cache_->Capture(feature);
  }
  const uint64_t start = pipeline.now();
  for (const auto& placement : context.placements) {
    if (!(placement.tile == context.tile)) continue;
    if (!orcmap::RenderOrderedFeatureAt(feature, placement, context.viewport,
                                orcmap::styles::OrcSdrDark(), &context.target,
                                &context.scratch)) return false;
    if (context.labels && feature.geometry.type == orcmap::GeomType::kPoint) {
      orcmap::TileScreenMap map;
      if (!orcmap::MakeTilePlacementScreenMap(context.viewport, placement,
              feature.extent ? feature.extent : 4096, &map)) return false;
      context.labels->OfferPlace(feature,map,context.label_zoom,orcmap::styles::OrcSdrDark());
    }
  }
  pipeline.stats_.render_us += pipeline.now() - start;
  return true;
}

bool Pipeline::render(const orcmap::Viewport& viewport, orcmap::RenderTarget& target,
                       orcmap::LabelLayout* labels, orcmap::LabelTextTarget* text,int label_zoom) {
  try {return render_impl(viewport,target,labels,text,label_zoom);}
  catch(const std::bad_alloc&) {if(feature_cache_)feature_cache_->Commit(false);render_scratch_.Reset();ready_=false;stats_.out_of_memory=true;++stats_.failed;return false;}
}
bool Pipeline::render_impl(const orcmap::Viewport& viewport,orcmap::RenderTarget& target,
                            orcmap::LabelLayout* labels,orcmap::LabelTextTarget* text,int label_zoom) {
  stats_ = {};
  const uint64_t start = now();
  if ((labels == nullptr) != (text == nullptr)) return false;
  if (label_zoom < -1 || label_zoom > 31) return false;
  if (label_zoom == -1) label_zoom = viewport.zoom;
  if (!ready_ || viewport.width_px != target.Width() || viewport.height_px != target.Height() ||
      viewport.zoom < reader_.Header().min_zoom || viewport.zoom > reader_.Header().max_zoom) return false;
  std::vector<orcmap::TilePlacement> placements;
  if (!orcmap::EnumerateVisibleTilePlacements(viewport, &placements) || placements.size() > 64) return false;
  const bool include_places = labels && label_zoom >= 6;
  if (feature_cache_) feature_cache_->Prepare(placements, include_places);
  if (!orcmap::ClearMapBackground(viewport, orcmap::styles::OrcSdrDark(), &target)) return false;
  if (labels) labels->BeginFrame(viewport.width_px,viewport.height_px);
  for (size_t i = 0; i < placements.size(); ++i) {
    const auto tile = placements[i].tile;
    bool duplicate = false;
    for (size_t j = 0; j < i; ++j) duplicate |= placements[j].tile == tile;
    if (duplicate) continue; // decode once, draw every wrapped-world placement
    const uint64_t lookup = now();
    const bool present = reader_.TileExists(tile.z, tile.x, tile.y);
    stats_.lookup_us += now() - lookup;
    if (!present) { ++stats_.missing; continue; }
    // Keep the world overview clear; place names begin at regional zoom.
    render_scratch_.Reset();
    Context context{*this, viewport, target, placements, tile,
                    static_cast<uint8_t>(label_zoom), label_zoom >= 6 ? labels : nullptr, render_scratch_};
    orcmap::MvtStreamOptions options;
    options.include_layer = [](const char* name,size_t size,void* opaque) {
      const auto& ctx=*static_cast<Context*>(opaque);
      const std::string_view layer(name,size);
      return (ctx.labels && (layer=="place" || layer=="places" || layer=="place_label")) ||
          orcmap::experimental::IncludeNoTextBasemapLayer(name,size,nullptr);
    };
    options.include_layer_ctx = &context;
    options.feature_sink = &Pipeline::feature;
    options.feature_sink_ctx = &context;
    const uint64_t stream = now(), render_before = stats_.render_us;
    const auto* replay = feature_cache_ ? feature_cache_->Find(tile) : nullptr;
    context.replaying = replay != nullptr;
    bool ok;
    if (replay) {
      ++stats_.replay_hits;
      ok = replay->Replay(scratch_.feature, &Pipeline::feature, &context);
    } else {
      if (feature_cache_) feature_cache_->Begin(tile, include_places);
      ok = reader_.StreamTile(tile.z, tile.x, tile.y, options, &scratch_);
    }
    stats_.stream_us += now() - stream - (stats_.render_us - render_before);
    if (ok) {
      const uint64_t flush = now();
      ok = orcmap::FinishOrderedTile(&render_scratch_, &target, hooks_.service, hooks_.context);
      stats_.render_us += now() - flush;
    }
    if (!replay && feature_cache_ && !feature_cache_->Commit(ok) && feature_cache_->Capacity()) ++stats_.replay_fallbacks;
    if (render_scratch_.status == orcmap::OrderedRenderStatus::kCancelled) stats_.cancelled = true;
    if (render_scratch_.status == orcmap::OrderedRenderStatus::kCapacityExceeded) ++stats_.deferred_capacity_failures;
    stats_.deferred_peak_bytes = std::max(stats_.deferred_peak_bytes, render_scratch_.peak_bytes);
    stats_.deferred_retained_bytes = render_scratch_.RetainedBytes();
    render_scratch_.Reset(); // Discard pending strokes on decode failure too.
    if (ok) ++stats_.tiles;
    else ++stats_.failed;
    if (stats_.cancelled) break;
  }
  if (labels && !stats_.cancelled && !stats_.failed) {
    if (hooks_.service && !hooks_.service(hooks_.context)) stats_.cancelled=true;
    else {
      const uint64_t label_start=now();
      const auto& style=orcmap::styles::OrcSdrDark();
      labels->Draw(*text,style.Rule(orcmap::FeatureKind::kLabelPrimary).color,
                   style.Rule(orcmap::FeatureKind::kBackground).color);
      stats_.label_us=now()-label_start;
    }
    stats_.labels=labels->stats();
  }
  stats_.total_us = now() - start;
  stats_.replay_bytes = feature_cache_ ? feature_cache_->Used() : 0;
  stats_.replay_capacity = feature_cache_ ? feature_cache_->Capacity() : 0;
  return stats_.tiles > 0 && stats_.failed == 0 && !stats_.cancelled;
}

bool begin_location_picker(orcmap::LocationPicker& picker,int width,int height,
                           int32_t latitude_e7,int32_t longitude_e7) {
  if(!coordinates(latitude_e7,longitude_e7)||width<=0||height<=0||width>1280||height>720)return false;
  orcmap::Viewport view;view.width_px=width;view.height_px=height;
  // GeoBounds stores longitude before latitude. Name the fields to avoid
  // silently rejecting initialization and retaining a default 256x256 view.
  orcmap::GeoBounds world;
  world.min_lon_deg=-180;world.max_lon_deg=180;
  world.min_lat_deg=-orcmap::kMercatorMaxLatDeg;world.max_lat_deg=orcmap::kMercatorMaxLatDeg;
  if(!picker.Begin(view,world,{0,20}))return false;
  (void)picker.ZoomIn(); // Start one level closer; retain the whole-world zoom-out floor.
  double x=0,y=0;
  return orcmap::ProjectLatLon(picker.View(),{latitude_e7/1e7,longitude_e7/1e7},&x,&y)&&picker.RecenterAtScreen(x,y);
}

bool pack_source_view(const orcmap::Viewport& requested,uint8_t minimum,uint8_t maximum,
                         orcmap::Viewport* output) {
  if(!output || minimum>maximum || maximum>31 || requested.zoom>20 ||
      requested.tile_size_px<=0)return false;
  auto view=requested;
  const int source=std::min(view.zoom,maximum);
  if(source<minimum)return false;
  const int delta=view.zoom-source;
  if(delta>16 || view.tile_size_px>(INT32_MAX>>delta))return false;
  view.tile_size_px<<=delta;view.zoom=static_cast<uint8_t>(source);
  *output=view;return true;
}

bool world_overview_view(const orcmap::Viewport& requested,uint8_t minimum,uint8_t maximum,
                         orcmap::Viewport* output) {
  if(minimum>maximum || maximum>20)return false;
  // Keep the embedded overview's one-level-coarser performance policy.
  const int source=std::clamp(static_cast<int>(requested.zoom)-1,static_cast<int>(minimum),static_cast<int>(maximum));
  return pack_source_view(requested,minimum,static_cast<uint8_t>(source),output);
}

bool regional_view(int32_t latitude_e7, int32_t longitude_e7, double range_nm,
                   int width, int height, orcmap::Viewport* output) {
  if (!output || !coordinates(latitude_e7, longitude_e7) || std::abs(static_cast<int64_t>(latitude_e7))>850511287 || width <= 0 || height <= 0 ||
      width > 1280 || height > 720 || !std::isfinite(range_nm) || range_nm <= 0 || range_nm > 10000) return false;
  orcmap::Viewport view;
  view.width_px = width; view.height_px = height;
  if (!orcmap::SetCenter(&view, latitude_e7 / 1e7, longitude_e7 / 1e7)) return false;
  // Choose source zoom and screen tile size so range has a continuous scale,
  // rather than changing the receiver's requested range to an integer zoom.
  const double radius = std::min(width, height) / 2.0;
  const double cosine = std::max(.05, std::cos(view.center_lat_deg * .017453292519943295));
  const double world_pixels = radius * 21600.0 * cosine / range_nm;
  const int zoom = std::clamp(static_cast<int>(std::floor(std::log2(std::max(1.0, world_pixels / 256.0)))), 0, 20);
  view.zoom = static_cast<uint8_t>(zoom);
  view.tile_size_px = std::max(1, static_cast<int>(std::round(world_pixels / std::ldexp(1.0, zoom))));
  *output = view;
  return true;
}

bool project(const orcmap::Viewport& view, int32_t latitude_e7, int32_t longitude_e7, int* x, int* y) {
  if (!x || !y || !coordinates(latitude_e7, longitude_e7) || std::abs(static_cast<int64_t>(latitude_e7))>850511287) return false;
  double sx = 0, sy = 0;
  if (!orcmap::ProjectLatLon(view, {latitude_e7 / 1e7, longitude_e7 / 1e7}, &sx, &sy) ||
      !std::isfinite(sx) || !std::isfinite(sy) || sx < 0 || sy < 0 || sx >= view.width_px || sy >= view.height_px) return false;
  *x = static_cast<int>(std::lround(sx)); *y = static_cast<int>(std::lround(sy));
  return *x < view.width_px && *y < view.height_px;
}
} // namespace orcsdr::maps
