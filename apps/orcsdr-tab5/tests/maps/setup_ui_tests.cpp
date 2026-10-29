#include "setup_ui.hpp"
#include "text_editor.hpp"
#include <M5Unified.h>
#include <cassert>
#include <string>
#include <cstdio>
using namespace orcsdr;
namespace {
setup_ui::Snapshot state;
int scans=0,saves=0;
std::string ssid,password,query;
bool saved_ok=true;
void snapshot(setup_ui::Snapshot* out){*out=state;}
void scan(){++scans;}
bool connect(const char* s,const char* p){ssid=s;password=p;return true;}
bool lookup(const char* s){query=s?s:"NETWORK";return state.connected;}
bool save(int32_t lat,int32_t lon){
  assert(!M5.Display.strings.empty()&&M5.Display.strings.back()=="SAVING...");
  ++saves;if(!saved_ok)return false;state.location_set=true;state.latitude_e7=lat;state.longitude_e7=lon;return true;
}
void type(const char* value){for(;*value;++value){keyboard_input::Key k;k.ch=*value;setup_ui::key(setup_ui::Page::location,k);}}
void enter(){keyboard_input::Key k;k.special=keyboard_input::Special::enter;setup_ui::key(setup_ui::Page::location,k);}
bool visible(const char* s){return std::find(M5.Display.strings.begin(),M5.Display.strings.end(),s)!=M5.Display.strings.end();}
}
int main(){
  setup_ui::configure({snapshot,scan,connect,lookup,save});setup_ui::reset();
  state.count=1;strlcpy(state.networks[0].ssid,"Bench",33);state.networks[0].secure=true;
  setup_ui::draw(setup_ui::Page::network);
  setup_ui::touch(setup_ui::Page::network,100,550);assert(scans==1);
  setup_ui::touch(setup_ui::Page::network,100,260);assert(setup_ui::input_active());
  type("test-password");enter();assert(ssid=="Bench"&&password=="test-password");
  assert(!setup_ui::input_active()&&std::string(text_editor::value()).empty());
  setup_ui::draw(setup_ui::Page::location);
  setup_ui::touch(setup_ui::Page::location,100,260);assert(query=="NETWORK");assert(saves==0);
  assert(setup_ui::back());
  state.connected=true;
  setup_ui::touch(setup_ui::Page::location,100,260);
  state.lookup_ready=true;state.lookup_latitude_e7=440500000;state.lookup_longitude_e7=-1230200000;
  assert(setup_ui::poll());setup_ui::draw(setup_ui::Page::location);
  assert(visible("USE THIS LOCATION"));saved_ok=false;
  setup_ui::touch(setup_ui::Page::location,800,530);assert(saves==1&&!state.location_set);
  M5.Display.strings.clear();setup_ui::draw(setup_ui::Page::location);
  assert(visible("Location could not be saved. Retry or go Back.")&&visible("USE THIS LOCATION"));
  assert(!visible("SAVING..."));
  saved_ok=true;setup_ui::touch(setup_ui::Page::location,800,530);assert(saves==2&&state.location_set);
  M5.Display.strings.clear();
  setup_ui::draw(setup_ui::Page::location);
  assert(visible("Saved: 44.050000, -123.020000")&&!visible("SAVING..."));
  setup_ui::touch(setup_ui::Page::location,800,260);type("97477");enter();assert(query=="97477");assert(saves==2);
  assert(setup_ui::back());
  assert(setup_ui::touch(setup_ui::Page::location,100,430)==setup_ui::Result::picker);
  setup_ui::touch(setup_ui::Page::location,800,430);assert(setup_ui::input_active());
  assert(setup_ui::back());assert(!setup_ui::input_active());assert(saves==2);
  setup_ui::reset();assert(!setup_ui::input_active());
  state.location_set=false;saved_ok=false;
  setup_ui::touch(setup_ui::Page::location,800,430);
  type("44.1");enter();type("-123.2");enter();
  assert(saves==2&&!state.location_set&&!setup_ui::input_active());
  M5.Display.strings.clear();setup_ui::draw(setup_ui::Page::location);
  assert(visible("Latitude: 44.100000")&&visible("Longitude: -123.200000"));
  assert(visible("SAVE LOCATION")&&!visible("SAVING..."));
  setup_ui::touch(setup_ui::Page::location,800,530);
  assert(saves==3&&!state.location_set&&!setup_ui::input_active());
  M5.Display.strings.clear();setup_ui::draw(setup_ui::Page::location);
  assert(visible("SAVE LOCATION")&&visible("Location could not be saved. Retry or go Back."));
  assert(!visible("SAVING..."));
  saved_ok=true;setup_ui::touch(setup_ui::Page::location,800,530);
  assert(saves==4&&state.location_set&&state.latitude_e7==441000000&&state.longitude_e7==-1232000000);
  M5.Display.strings.clear();setup_ui::draw(setup_ui::Page::location);
  assert(visible("Saved: 44.100000, -123.200000")&&!visible("SAVING..."));
  // Abandon a second proposal without replacing the saved point.
  setup_ui::touch(setup_ui::Page::location,800,430);
  type("-33.9");enter();type("151.2");enter();
  assert(saves==4&&state.latitude_e7==441000000&&state.longitude_e7==-1232000000);
  assert(setup_ui::back());
  assert(saves==4&&state.latitude_e7==441000000&&state.longitude_e7==-1232000000);
  // Delayed wizard teardown must not close a newer editor owned by Settings.
  setup_ui::reset();setup_ui::touch(setup_ui::Page::network,100,260);
  type("abandoned-password");
  assert(text_editor::active());
  setup_ui::reset();
  assert(!text_editor::active()&&std::string(text_editor::value()).empty());
  setup_ui::touch(setup_ui::Page::network,100,260);type("old-password");
  text_editor::begin("NEW OWNER","keep this",30,false,"SAVE");
  setup_ui::reset();
  if(!text_editor::active()||std::string(text_editor::value())!="keep this") {
    std::puts("FAIL: wizard teardown replaced newer editor");return 1;
  }
  text_editor::close();
  std::puts("SETUP_UI_PASS owned inputs, password clearing, offline lookup, confirm/failure/cancel");
}
