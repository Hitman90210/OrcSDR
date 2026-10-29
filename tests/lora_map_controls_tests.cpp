// Real dashboard, gesture bindings, serial parser, map adapter and OrcMaps camera.
// Only device/display, receiver location, persistence and the async worker are faked.
#include "lora_dashboard.hpp"
#include "map_view.hpp"
#include "map_preferences.hpp"
#include "receiver_location.hpp"
#include "screen_controller.hpp"
#include <M5Unified.h>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
namespace {
orcsdr::receiver_location::Snapshot location;
orcmap::Viewport painted_camera;
bool owner=true,labels=true;
bool label_write_ok=true,painted_labels=true;
int frames=0;
uint32_t map_revision=1;
std::string reply;
void command(const char* text,bool auth=true) {
  reply.clear();
  assert(orcsdr::lora::handle_map_command(text,auth,+[](const char* s){reply+=s;}));
}
void ok(const char* text){command(text);assert(reply=="RTL_LORA_MAP_OK\n");}
bool painted(const char* text) {
  for(const auto& p:lgfx::paints)if(p.text==text)return true;
  return false;
}
}
namespace orcsdr::screens {bool owns(Id id){return owner&&id==Id::lora;}}
namespace orcsdr::receiver_location {Snapshot snapshot(){return location;}}
namespace orcsdr::audio_header {
void draw_brand(const char*){} void draw_home_button(){} void draw_battery(int32_t){}
void draw_mute_button(bool){} void draw_visualizer_button(bool){} void draw_settings_button(){}
bool home_hit(int32_t,int32_t){return false;} bool settings_hit(int32_t,int32_t){return false;}
bool self_check(){return true;}
}
namespace orcsdr::lora_channel {
uint8_t survey_span_count(){return 3;} bool self_check(){return true;}
}
namespace orcsdr::maps::preferences {
bool labels(Consumer){return ::labels;}
bool set_labels(Consumer,bool value){if(!label_write_ok)return false;::labels=value;return true;}
}
namespace orcsdr::maps {
uint32_t revision(){return map_revision;}
bool regional_view(int32_t lat,int32_t lon,double range,int w,int h,orcmap::Viewport* out) {
  assert(range==15.0&&w==842&&h==330);
  out->width_px=w;out->height_px=h;out->zoom=8;
  return orcmap::SetCenter(out,lat/1e7,lon/1e7);
}
bool project(const orcmap::Viewport& view,int32_t lat,int32_t lon,int* x,int* y) {
  double sx=0,sy=0;
  if(!orcmap::ProjectLatLon(view,{lat/1e7,lon/1e7},&sx,&sy)||sx<0||sy<0||sx>=view.width_px||sy>=view.height_px)return false;
  *x=std::lround(sx);*y=std::lround(sy);return true;
}
Frame draw(const orcmap::Viewport& camera,lgfx::LovyanGFX&,int x,int y,FrameInfo* info,const char*,bool show_labels) {
  assert(x==50&&y==182);painted_camera=camera;painted_labels=show_labels;++frames;
  std::strcpy(info->credit,"Test map attribution");return Frame::pending;
}
}
int main() {
  using namespace orcsdr::lora;
  Snapshot snapshot;snapshot.node_count=2;snapshot.selected_node=0;
  snapshot.nodes[0].id=0x11111111;snapshot.nodes[1].id=0x22222222;
  snapshot.nodes[0].latitude_e7=100000000;snapshot.nodes[0].longitude_e7=1799000000;
  std::strcpy(snapshot.nodes[0].name,"NODE A");
  snapshot.nodes[1].latitude_e7=INT32_MAX;snapshot.nodes[1].longitude_e7=INT32_MAX;
  location.configured=true;location.latitude_e7=100000000;location.longitude_e7=1798000000;
  show_documentation_view(View::map,snapshot);
  auto state=map_state();assert(state.ready&&!state.following&&state.longitude_e7==location.longitude_e7);
  const int label_clears=lgfx::screen_clears;
  for(int i=0;i<4;++i) {
    lgfx::paints.clear();handle_touch(500,600);
    assert(lgfx::screen_clears==label_clears&&painted_labels==labels);
    assert(painted(labels?"LABELS ON":"LABELS OFF"));
  }
  label_write_ok=false;lgfx::paints.clear();handle_touch(500,600);
  assert(lgfx::screen_clears==label_clears&&painted("SAVE FAILED"));
  label_write_ok=true;handle_touch(500,600);
  assert(lgfx::screen_clears==label_clears&&painted_labels==labels);
  const int initial_frames=frames;
  command("RTL_LORA_MAP PAN 20 10",false);assert(reply.find("auth_required")!=std::string::npos&&frames==initial_frames);
  command("RTL_LORA_MAP STATUS",false);assert(reply.find("active=1 ready=1")!=std::string::npos);
  assert(reply.find("latitude_e7=")==std::string::npos&&reply.find("longitude_e7=")==std::string::npos);
  assert(reply.find("following_node=")==std::string::npos);
  for(const char* text:{"RTL_LORA_MAP PAN 1 2 extra","RTL_LORA_MAP ZOOM IN extra","RTL_LORA_MAP PAN 9999999999999999 0"}) {
    command(text);assert(reply.find("invalid_command")!=std::string::npos&&frames==initial_frames);
  }
  command("RTL_LORA_MAP PAN 843 0");assert(reply.find("rejected")!=std::string::npos&&frames==initial_frames);
  owner=false;command("RTL_LORA_MAP CENTER NODE");assert(reply.find("rejected")!=std::string::npos);
  assert(!map_touch(400,300,true,true));owner=true;
  ok("RTL_LORA_MAP CENTER NODE");assert(map_state().longitude_e7==1799000000);
  ok("RTL_LORA_MAP FOLLOW ON");assert(map_state().following&&map_state().following_node_id==0x11111111);
  command("RTL_LORA_MAP STATUS");assert(reply.find("following_node=11111111")!=std::string::npos);
  ++snapshot.revision;snapshot.nodes[0].longitude_e7=1799500000;test_millis+=1000;update(snapshot);
  assert(map_state().longitude_e7==1799500000&&map_state().following);
  // New traffic moves rows: Follow must retain device identity, not row zero.
  snapshot.nodes[1].latitude_e7=200000000;snapshot.nodes[1].longitude_e7=300000000;
  std::swap(snapshot.nodes[0],snapshot.nodes[1]);
  ++snapshot.revision;test_millis+=1000;update(snapshot);
  assert(map_state().longitude_e7==1799500000&&map_state().following);
  snapshot.nodes[1].longitude_e7=1799600000;
  ++snapshot.revision;test_millis+=1000;update(snapshot);
  assert(map_state().longitude_e7==1799600000);
  snapshot.node_count=1;lgfx::paints.clear();
  ++snapshot.revision;test_millis+=1000;update(snapshot);
  assert(map_state().longitude_e7==1799600000&&map_state().following);
  assert(map_state().following_node_id==0x11111111);
  assert(painted("WAITING POSITION"));
  snapshot.node_count=2;snapshot.nodes[1].longitude_e7=1799700000;
  ++snapshot.revision;test_millis+=1000;update(snapshot);
  assert(map_state().longitude_e7==1799700000);
  snapshot.nodes[1].latitude_e7=INT32_MAX;lgfx::paints.clear();
  ++snapshot.revision;test_millis+=1000;update(snapshot);
  assert(map_state().longitude_e7==1799700000&&painted("WAITING POSITION"));
  snapshot.nodes[1].latitude_e7=100000000;
  // Restore the original row/position fixture for the drag/parity checks.
  std::swap(snapshot.nodes[0],snapshot.nodes[1]);
  snapshot.nodes[0].longitude_e7=1799500000;
  snapshot.nodes[1].latitude_e7=INT32_MAX;snapshot.nodes[1].longitude_e7=INT32_MAX;
  ++snapshot.revision;test_millis+=1000;update(snapshot);
  const auto before=map_state();
  assert(map_touch(400,300,true,true));assert(map_state().longitude_e7==before.longitude_e7);
  assert(map_touch(402,301,true,false));assert(map_state().following);
  assert(map_touch(440,310,true,false));assert(!map_state().following&&map_state().following_node_id==0&&map_state().longitude_e7!=before.longitude_e7);
  const auto after=map_state();assert(map_touch(440,310,false,false));
  assert(map_state().longitude_e7==after.longitude_e7);
  // Serial pan and the equivalent physical drag must produce the same camera.
  ok("RTL_LORA_MAP CENTER NODE");ok("RTL_LORA_MAP PAN -40 -10");
  assert(map_state().longitude_e7==after.longitude_e7&&map_state().latitude_e7==after.latitude_e7);
  const auto zoom=map_state().zoom;ok("RTL_LORA_MAP ZOOM IN");assert(map_state().zoom==zoom+1);
  assert(!map_touch(850,210,true,true));handle_touch(850,210);assert(map_state().zoom==zoom+2);
  handle_touch(850,270);assert(map_state().zoom==zoom+1);
  ok("RTL_LORA_MAP ZOOM OUT");assert(map_state().zoom==zoom);
  ok("RTL_LORA_MAP CENTER RECEIVER");assert(map_state().longitude_e7==location.longitude_e7);
  // Wrap the antimeridian without an invented fallback location.
  ok("RTL_LORA_MAP CENTER NODE");ok("RTL_LORA_MAP PAN 200 0");assert(map_state().longitude_e7<0);
  snapshot.selected_node=1;++snapshot.revision;test_millis+=1000;update(snapshot);
  const auto retained=map_state();command("RTL_LORA_MAP FOLLOW ON");
  assert(reply.find("rejected")!=std::string::npos&&map_state().longitude_e7==retained.longitude_e7);
  // Overlay placement must follow the same camera and map origin after motion.
  snapshot.selected_node=0;++snapshot.revision;test_millis+=1000;update(snapshot);
  lgfx::paints.clear();ok("RTL_LORA_MAP CENTER NODE");assert(painted("NODE A")&&painted("RX"));
  double px=0,py=0;assert(orcmap::ProjectLatLon(painted_camera,{10,179.95},&px,&py));
  bool aligned=false;
  for(const auto& p:lgfx::paints)if(p.text=="NODE A") {
    aligned|=p.x==50+std::lround(px)+14&&p.y==182+std::lround(py)-6;
    assert(p.text_size==2);
  }
  assert(aligned);
  // Unrelated telemetry/age ticks must not erase and repaint a stationary map.
  const int unchanged_frames=frames;
  for(int i=0;i<3;++i) {
    ++snapshot.revision;++snapshot.uptime_seconds;++snapshot.crc_ok;
    test_millis+=1000;update(snapshot);
  }
  assert(frames==unchanged_frames);
  // Colocated nodes keep every marker but never print names over one another.
  snapshot.node_count=kNodeCapacity;snapshot.selected_node=kNodeCapacity-1;
  for(size_t i=0;i<kNodeCapacity;++i) {
    snapshot.nodes[i]=snapshot.nodes[0];snapshot.nodes[i].id=0x1000+i;
    std::snprintf(snapshot.nodes[i].name,sizeof(snapshot.nodes[i].name),"CLUSTER %02u",unsigned(i));
  }
  lgfx::paints.clear();++snapshot.revision;test_millis+=1000;update(snapshot);
  assert(painted("CLUSTER 15"));
  std::vector<lgfx::Paint> names;
  for(const auto& p:lgfx::paints)if(p.text.rfind("CLUSTER ",0)==0&&p.x<892&&p.y>=182&&p.y<488) {
    assert(p.text_size==2&&p.x>=54&&p.x+int(p.text.size())*12<=888&&p.y-12>=186&&p.y+12<=488);
    for(const auto& before:names)assert(p.x>=before.x+int(before.text.size())*12+2||
        before.x>=p.x+int(p.text.size())*12+2||p.y-12>=before.y+14||before.y-12>=p.y+14);
    names.push_back(p);
  }
  assert(names.size()>1&&names.size()<kNodeCapacity&&names.front().text=="CLUSTER 15");
  snapshot.node_count=2;snapshot.selected_node=0;
  snapshot.nodes[0].id=0x11111111;std::strcpy(snapshot.nodes[0].name,"NODE A");
  snapshot.nodes[1].latitude_e7=INT32_MAX;snapshot.nodes[1].longitude_e7=INT32_MAX;
  ++snapshot.revision;test_millis+=1000;update(snapshot);
  // A completed worker frame repaints even without a new radio snapshot.
  const auto before_publication=frames;++map_revision;service_map();assert(frames==before_publication+1);
  owner=false;++map_revision;service_map();assert(frames==before_publication+1);owner=true;
  service_map();assert(frames==before_publication+2);
  snapshot.nodes[0].latitude_e7=900000000;++snapshot.revision;test_millis+=1000;update(snapshot);
  command("RTL_LORA_MAP CENTER NODE");assert(reply.find("rejected")!=std::string::npos);
  snapshot.nodes[0].latitude_e7=100000000;++snapshot.revision;test_millis+=1000;update(snapshot);
  for(int i=0;i<20;++i)map_control(MapControl::zoom_in);assert(map_state().zoom==16);
  for(int i=0;i<20;++i)map_control(MapControl::zoom_out);assert(map_state().zoom==0);
  leave();const int left_frames=frames;command("RTL_LORA_MAP ZOOM IN");assert(reply.find("rejected")!=std::string::npos&&frames==left_frames);
  location.configured=false;snapshot.node_count=0;show_documentation_view(View::map,snapshot);
  assert(!map_state().ready&&painted("NO MAPPABLE POSITION"));
  command("RTL_LORA_MAP CENTER RECEIVER");assert(reply.find("rejected")!=std::string::npos);
  leave();std::puts("LORA_MAP_CONTROLS_PASS shared camera, drag/serial parity, projection, follow, ownership and invalid input");
}
