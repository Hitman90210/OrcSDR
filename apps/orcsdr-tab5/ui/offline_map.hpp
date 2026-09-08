#pragma once

#include <cstddef>
#include <cstdint>

#include "orcsdr_storage.hpp"

namespace lgfx { inline namespace v1 { class LovyanGFX; } }

namespace orcsdr::offline_map {

// The only map pack the signed catalog publishes is the upstream author's
// home county, which is useless to everyone else. A user-supplied map at
// kUserPath (built with tools/build_orcmap.py from any OSM extract and copied
// to the SD card) is loaded in preference to it, so "offline map" means the
// user's own area rather than Lane County, Oregon.
constexpr const char kUserPath[] = "/orcsdr/data/local_map.idx";
constexpr const char kRuntimePath[] = "/orcsdr/data/lane_county_map.idx";

struct View {
  float center_lat = 0.0f;
  float center_lon = 0.0f;
  float range_nm = 25.0f;
  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;
};

// Normal UI-code only. This bounded cache is never touched by SDR/audio callbacks.
bool load(orcsdr::storage::FileSystem* filesystem);
bool available();
void draw_base(const View& view, uint16_t water_color, uint16_t road_color,
               uint16_t airport_color, uint16_t border_color);
void draw_base(lgfx::v1::LovyanGFX& display, const View& view, uint16_t water_color,
               uint16_t road_color, uint16_t airport_color, uint16_t border_color);
bool project(const View& view, float latitude, float longitude, int* x, int* y);
bool self_check();

}  // namespace orcsdr::offline_map
