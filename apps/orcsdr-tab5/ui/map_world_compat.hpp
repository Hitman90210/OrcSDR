#pragma once
#include <cmath>
namespace lgfx { inline namespace v1 { class LovyanGFX; } }
namespace orcsdr::maps {
inline bool world_compat_project(double latitude,double longitude,int left,int top,int width,int height,int* x,int* y) {
  if(!x||!y||width<=0||height<=0||width>1280||height>720||!std::isfinite(latitude)||
      !std::isfinite(longitude)||latitude<-90||latitude>90||longitude<-180||longitude>180)return false;
  // Preserve the existing full-world float geometry, including unclipped edges.
  *x=left+static_cast<int>((static_cast<float>(longitude)+180.0f)/360.0f*width);
  *y=top+static_cast<int>((90.0f-static_cast<float>(latitude))/180.0f*height);
  return true;
}
// Compatibility for whole-globe Maidenhead displays, including polar grids.
// OrcMaps currently supplies Mercator only. Keep this explicit until its core
// supports a whole-globe projection; do not misalign locators over a new base.
void draw_world_compat(lgfx::v1::LovyanGFX&,int sub_x,int sub_y,int left,int top,int width,int height);
}
