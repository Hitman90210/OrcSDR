#include "map_pipeline.hpp"
#include "file_byte_source.hpp"
#include "framebuffer_target.hpp"
#include <cassert>
#include <cstdio>
#include <cmath>
#include <utility>
#include <algorithm>
#include <string>
#include <vector>

namespace {
struct WorldText final : orcmap::LabelTextTarget {
  std::vector<std::string> names;
  bool MeasureText(std::string_view name,orcmap::TextExtent* extent) override {
    *extent={static_cast<int>(name.size())*12,28};return !name.empty();
  }
  void DrawText(int,int,std::string_view name,orcmap::Color,orcmap::Color) override {
    names.emplace_back(name);
  }
};
void world_label_zoom_test(orcsdr::maps::Pipeline& pipeline) {
  orcmap::LabelLayout labels;
  WorldText text;
  orcmap::host::FramebufferTarget frame(622,302);
  for (const auto& city : {orcmap::LatLon{51.51,-0.13},orcmap::LatLon{44.94,-123.03}}) {
    orcmap::Viewport requested,source;
    assert(orcsdr::maps::regional_view(static_cast<int32_t>(std::llround(city.lat_deg*1e7)),
        static_cast<int32_t>(std::llround(city.lon_deg*1e7)),100,622,302,&requested));
    assert(requested.zoom>=6);
    assert(orcsdr::maps::world_overview_view(requested,0,4,&source) && source.zoom==4);
    text.names.clear();
    assert(pipeline.render(source,frame,&labels,&text));
    assert(text.names.empty() && pipeline.stats().labels.offered==0);
    assert(pipeline.render(source,frame,&labels,&text,requested.zoom));
    const char* expected=city.lat_deg>50 ? "London" : "Salem";
    assert(std::find(text.names.begin(),text.names.end(),expected)!=text.names.end());
    std::printf("WORLD_LOGICAL_LABEL_PASS city=%s logical_zoom=%u source_zoom=%u drawn=%u\n",
        expected,requested.zoom,source.zoom,pipeline.stats().labels.drawn);
    text.names.clear();
    assert(pipeline.render(source,frame,nullptr,nullptr,requested.zoom));
    assert(text.names.empty() && pipeline.stats().labels.offered==0 && pipeline.stats().labels.drawn==0);
    assert(pipeline.render(source,frame,&labels,&text,5));
    assert(text.names.empty() && pipeline.stats().labels.offered==0);
    assert(!pipeline.render(source,frame,&labels,&text,-2));
    assert(!pipeline.render(source,frame,&labels,&text,32));
  }
}
void source_camera_test() {
  using orcsdr::maps::pack_source_view;
  for(double longitude:{20.0,179.99,-179.99}) {
    orcmap::Viewport requested;
    requested.center_lat_deg=10;requested.center_lon_deg=longitude;
    requested.width_px=622;requested.height_px=302;requested.tile_size_px=384;
    for(uint8_t zoom:{uint8_t{6},uint8_t{8},uint8_t{12},uint8_t{20}}) {
      requested.zoom=zoom;orcmap::Viewport source;
      assert(pack_source_view(requested,0,8,&source));
      assert(source.zoom==std::min<int>(zoom,8));
      assert(source.tile_size_px==(requested.tile_size_px<<(zoom-source.zoom)));
      for(const auto point:{orcmap::LatLon{10,longitude},orcmap::LatLon{10.01,longitude+.01}}) {
        double ax,ay,bx,by;
        assert(orcmap::ProjectLatLon(requested,point,&ax,&ay));
        assert(orcmap::ProjectLatLon(source,point,&bx,&by));
        assert(std::abs(ax-bx)<1e-7&&std::abs(ay-by)<1e-7);
      }
    }
  }
  orcmap::Viewport requested,source;requested.zoom=3;
  assert(!pack_source_view(requested,5,8,&source));
  assert(!pack_source_view(requested,9,8,&source));
  assert(!pack_source_view(requested,0,32,&source));
  assert(!pack_source_view(requested,0,8,nullptr));
  requested.zoom=20;requested.tile_size_px=INT32_MAX;
  assert(!pack_source_view(requested,0,8,&source));
  std::puts("PACK_SOURCE_CAMERA_PASS projection retained through source z8, wrap and invalid bounds");
}
}

void world_tests(const char* path) {
  source_camera_test();
  orcmap::host::FileByteSource source(path);
  orcsdr::maps::Pipeline pipeline(source);
  assert(pipeline.open("orcmaps-overview-1"));
  assert(pipeline.header().min_zoom==0&&pipeline.header().max_zoom==4);
  world_label_zoom_test(pipeline);
  for(auto size:{std::pair<int,int>{900,330},{1248,568}}) {
    orcmap::Viewport view;
    orcmap::host::FramebufferTarget frame(size.first,size.second);
    for(double longitude:{-123.022,0.0,179.99}) {
      orcmap::LocationPicker picker;
      assert(orcsdr::maps::begin_location_picker(picker,size.first,size.second,440500000,
          static_cast<int32_t>(std::llround(longitude*1e7))));
      view=picker.View();
      int x=0,y=0;
      assert(orcsdr::maps::project(view,440500000,static_cast<int32_t>(std::llround(longitude*1e7)),&x,&y));
      assert(std::abs(x-size.first/2)<=1 && std::abs(y-size.second/2)<=1);
      assert(pipeline.render(view,frame));
      const auto pixels=frame.Pixels();
      assert(pipeline.render(view,frame));assert(frame.Pixels()==pixels);
      orcmap::Viewport overview;
      assert(orcsdr::maps::world_overview_view(view,pipeline.header().min_zoom,pipeline.header().max_zoom,&overview));
      for(const auto point:{orcmap::LatLon{44.05,longitude},orcmap::LatLon{20,longitude+1}}) {
        double detailed_x,detailed_y,overview_x,overview_y;
        assert(orcmap::ProjectLatLon(view,point,&detailed_x,&detailed_y));
        assert(orcmap::ProjectLatLon(overview,point,&overview_x,&overview_y));
        assert(std::abs(detailed_x-overview_x)<1e-7&&std::abs(detailed_y-overview_y)<1e-7);
      }
      assert(pipeline.render(overview,frame));
      assert(pipeline.stats().missing==0&&pipeline.stats().failed==0);
      std::printf("WORLD_VIEW size=%dx%d lon=%.3f tiles=%u features=%u bytes=%llu\n",size.first,size.second,longitude,
          pipeline.stats().tiles,pipeline.stats().features,
          static_cast<unsigned long long>(pipeline.stats().bytes));
    }
    assert(frame.WritePpm(size.first==1248?"map-world-large.ppm":"map-world-antimeridian.ppm"));
  }
  std::puts("MAP_WORLD_TEST_PASS (host geometry; not a P4 performance result)");
}
