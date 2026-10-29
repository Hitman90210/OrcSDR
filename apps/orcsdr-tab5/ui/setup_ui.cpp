#include "setup_ui.hpp"
#include "text_editor.hpp"
#include "focus_nav.hpp"
#include <M5Unified.h>
#include <esp_attr.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace orcsdr::setup_ui {
namespace {
constexpr uint16_t bg=0x0841,panel=0x1082,ink=0xffff,cyan=0x2e7f,green=0x6fe8;
Hooks g_hooks;
EXT_RAM_BSS_ATTR Snapshot g_state,g_next;
enum class View {methods,lookup,coordinates};
enum class Input {none,password,address,latitude,longitude};
View g_view=View::methods;
Input g_input=Input::none;
uint64_t g_editor_generation=0;
char g_ssid[33]{},g_error[96]{};
int g_scroll=0;
bool g_coordinates_ready=false,g_lookup_allowed=false;
int32_t g_latitude=0,g_longitude=0;

bool hit(int x,int y,int bx,int by,int w,int h){return x>=bx&&x<bx+w&&y>=by&&y<by+h;}
void text(const char* value,int x,int y,int size=4,uint16_t color=ink){
  M5.Display.setTextDatum(top_left);M5.Display.setTextSize(size);M5.Display.setTextColor(color,bg);
  M5.Display.drawString(value,x,y);
}
void button(const char* value,int x,int y,int w,int h=80,bool primary=false){
  focus_nav::note(x,y,w,h);
  M5.Display.fillRoundRect(x,y,w,h,8,panel);M5.Display.drawRoundRect(x,y,w,h,8,primary?green:cyan);
  M5.Display.setTextDatum(middle_left);M5.Display.setTextSize(3);M5.Display.setTextColor(primary?green:cyan,panel);
  M5.Display.drawString(value,x+16,y+h/2);
}
void refresh(){if(g_hooks.snapshot)g_hooks.snapshot(&g_state);g_state.count=std::min<uint8_t>(g_state.count,6);}
void clear_editor(){
  if(g_input!=Input::none&&text_editor::generation()==g_editor_generation){
    text_editor::begin("","",0,false,"");text_editor::close();
  }
  g_input=Input::none;g_editor_generation=0;
}
void edit(Input input,const char* title,const char* initial,size_t capacity,bool masked=false){
  g_input=input;text_editor::begin(title,initial,capacity,masked,input==Input::password?"CONNECT":"CONTINUE");
  g_editor_generation=text_editor::generation();
  text_editor::draw();
}
bool decimal(const char* value,bool latitude,int32_t* out){
  if(!value||!value[0]||value[0]==' '||value[0]=='\t')return false;
  char* end=nullptr;const double number=std::strtod(value,&end);
  if(!end||*end||!std::isfinite(number)||std::abs(number)>(latitude?90:180))return false;
  *out=static_cast<int32_t>(std::llround(number*1e7));return true;
}
bool save(int32_t latitude,int32_t longitude){
  // Show feedback before the synchronous save hook; a failed-save redraw
  // restores the original action without changing the proposed coordinates.
  button("SAVING...",680,520,560,80,true);
  if(!g_hooks.save||!g_hooks.save(latitude,longitude)){
    strlcpy(g_error,"Location could not be saved. Retry or go Back.",sizeof(g_error));return false;
  }
  g_error[0]=0;g_view=View::methods;refresh();return true;
}
void accepted(){
  const auto input=g_input;
  if(input==Input::password){
    // The connection hook copies credentials into the existing connection request.
    const bool ok=g_hooks.connect&&g_hooks.connect(g_ssid,text_editor::value());
    clear_editor();if(!ok)strlcpy(g_error,"Connection could not start. Please retry.",sizeof(g_error));
  } else if(input==Input::address){
    const bool ok=text_editor::value()[0]&&g_hooks.lookup&&g_hooks.lookup(text_editor::value());
    clear_editor();g_view=View::lookup;g_lookup_allowed=ok;
    if(!ok)strlcpy(g_error,"Address lookup needs Wi-Fi. Retry or use the map.",sizeof(g_error));
  } else if(input==Input::latitude||input==Input::longitude){
    int32_t value=0;
    if(!decimal(text_editor::value(),input==Input::latitude,&value)){
      char retry[17];strlcpy(retry,text_editor::value(),sizeof(retry));
      text_editor::begin(input==Input::latitude?"INVALID LATITUDE: -90 TO 90":"INVALID LONGITUDE: -180 TO 180",retry,16,false,"CONTINUE");
      g_editor_generation=text_editor::generation();
      return;
    }
    clear_editor();g_error[0]=0;
    if(input==Input::latitude){g_latitude=value;edit(Input::longitude,"SETUP: LONGITUDE (-180 TO 180)","",16);}
    else {g_longitude=value;g_coordinates_ready=true;}
  }
}
void draw_methods(){
  text("Set receiver location",32,122,5);
  text("Choose a method. Confirm the result to save.",32,180,3);
  button("USE NETWORK LOCATION",32,220,560,128);
  text("Approximate area; internet required",48,304,3);
  button("SEARCH ZIP / ADDRESS",656,220,592,128);
  text("Find your town, postcode or street",672,304,3);
  button("CHOOSE ON MAP",32,390,560,128);
  text("Pan and zoom; works offline",48,474,3);
  button("ENTER COORDINATES",656,390,592,128);
  text("Latitude and longitude in degrees",672,474,3);
  char line[96];
  if(g_state.location_set)std::snprintf(line,sizeof(line),"Saved: %.6f, %.6f",g_state.latitude_e7/1e7,g_state.longitude_e7/1e7);
  else strlcpy(line,"Location not set. You may skip this step.",sizeof(line));
  text(line,32,562,3,g_state.location_set?green:ink);
}
void draw_network(){
  text("Connect to Wi-Fi",32,122,5);text("Optional: connect for address search and map downloads.",32,180,3);
  g_scroll=std::clamp(g_scroll,0,std::max(0,static_cast<int>(g_state.count)-3));
  for(int row=0;row<3&&g_scroll+row<g_state.count;++row){
    const auto& network=g_state.networks[g_scroll+row];
    button(network.ssid,32,236+row*88,600,80);
  }
  if(!g_state.count)text(g_state.scanning?"Scanning networks...":"Tap SCAN to find networks.",32,260,3);
  button(g_state.scanning?"SCANNING...":"SCAN",32,520,240);
  button("UP",292,520,150);button("DOWN",462,520,170);
  if(g_state.connected){
    text("Connected",680,244,4,green);text(g_state.ssid,680,304,3);
    text("Continue when ready.",680,370,3);
  } else {
    text(g_state.connecting?"Connecting...":"Select a network",680,244,4);
    text("Enter its password here.",680,304,3);
    text("You stay inside setup.",680,352,3);
  }
  if(g_state.message[0]){char message[32];std::snprintf(message,sizeof(message),"%.31s",g_state.message);text(message,680,426,3);}
}
}
void configure(const Hooks& hooks){g_hooks=hooks;}
void reset(){if(g_input!=Input::none)clear_editor();g_view=View::methods;g_error[0]=0;g_ssid[0]=0;g_scroll=0;g_coordinates_ready=false;g_lookup_allowed=false;refresh();}
bool poll(){
  if(!g_hooks.snapshot)return false;
  g_next=Snapshot{};g_hooks.snapshot(&g_next);
  const bool changed=std::memcmp(&g_state,&g_next,sizeof(g_state))!=0;
  g_state=g_next;return changed;
}
void draw(Page page){
  refresh();
  if(g_input!=Input::none){text_editor::draw();return;}
  if(page==Page::network)draw_network();
  else if(g_view==View::methods)draw_methods();
  else if(g_view==View::coordinates){
    text("Enter your coordinates",32,122,5);
    text("Decimal degrees. South and west use minus (-).",32,190,3);
    button("ENTER COORDINATES",32,260,560);
    if(g_coordinates_ready){
      char line[64];
      std::snprintf(line,sizeof(line),"Latitude: %.6f",g_latitude/1e7);
      text(line,32,370,4);
      std::snprintf(line,sizeof(line),"Longitude: %.6f",g_longitude/1e7);
      text(line,32,430,4);
      text("Review this point, then save.",32,500,3);
      button("SAVE LOCATION",680,520,560,80,true);
    }
  } else {
    text("Confirm receiver location",32,122,5);
    text(g_state.lookup_busy?"Looking up location...":g_state.lookup_message,32,190,3);
    if(g_state.lookup_ready&&g_lookup_allowed){
      text(g_state.lookup_label,32,270,4);
      char line[96];std::snprintf(line,sizeof(line),"%.6f, %.6f",g_state.lookup_latitude_e7/1e7,g_state.lookup_longitude_e7/1e7);
      text(line,32,340,4);
      text("Review this point before saving.",32,410,3);
      button("USE THIS LOCATION",680,520,560,80,true);
    }else text("Go Back for another method or retry.",32,320,3);
  }
  if(g_error[0])text(g_error,32,608,3,TFT_ORANGE);
}
Result touch(Page page,int x,int y){
  if(g_input!=Input::none){
    const auto result=text_editor::handle_touch(x,y);
    if(result==text_editor::Result::accepted)accepted();
    else if(result==text_editor::Result::cancelled)clear_editor();
    return Result::redraw;
  }
  refresh();g_error[0]=0;
  if(page==Page::network){
    if(hit(x,y,32,520,240,80)&&!g_state.scanning){if(g_hooks.scan)g_hooks.scan();}
    else if(hit(x,y,292,520,150,80))g_scroll=std::max(0,g_scroll-1);
    else if(hit(x,y,462,520,170,80))g_scroll=std::min(std::max(0,static_cast<int>(g_state.count)-3),g_scroll+1);
    else for(int row=0;row<3&&g_scroll+row<g_state.count;++row)if(hit(x,y,32,236+row*88,600,80)){
      const auto& network=g_state.networks[g_scroll+row];strlcpy(g_ssid,network.ssid,sizeof(g_ssid));
      if(network.secure)edit(Input::password,"SETUP: WI-FI PASSWORD","",63,true);
      else if(g_hooks.connect&&!g_hooks.connect(g_ssid,""))strlcpy(g_error,"Connection could not start.",sizeof(g_error));
      break;
    }
    return Result::redraw;
  }
  if(g_view==View::methods){
    if(hit(x,y,32,220,560,128)){
      g_view=View::lookup;
      g_lookup_allowed=g_hooks.lookup&&g_hooks.lookup(nullptr);
      if(!g_lookup_allowed)strlcpy(g_error,"Network lookup needs Wi-Fi. Back for other methods.",sizeof(g_error));
    } else if(hit(x,y,656,220,592,128))edit(Input::address,"SETUP: ZIP CODE OR ADDRESS","",63);
    else if(hit(x,y,32,390,560,128))return Result::picker;
    else if(hit(x,y,656,390,592,128)){g_view=View::coordinates;g_coordinates_ready=false;edit(Input::latitude,"SETUP: LATITUDE (-90 TO 90)","",16);}
  } else if(g_view==View::lookup&&g_state.lookup_ready&&g_lookup_allowed&&hit(x,y,680,520,560,80))save(g_state.lookup_latitude_e7,g_state.lookup_longitude_e7);
  else if(g_view==View::coordinates){
    if(hit(x,y,32,260,560,80)){g_coordinates_ready=false;edit(Input::latitude,"SETUP: LATITUDE (-90 TO 90)","",16);}
    else if(g_coordinates_ready&&hit(x,y,680,520,560,80))save(g_latitude,g_longitude);
  }
  return Result::redraw;
}
Result key(Page page,const keyboard_input::Key& key){
  (void)page;
  if(g_input==Input::none)return Result::none;
  const auto action=text_editor::handle_key(key);
  if(action==keyboard_input::EditAction::accept)accepted();
  else if(action==keyboard_input::EditAction::cancel)clear_editor();
  return Result::redraw;
}
bool input_active(){return g_input!=Input::none;}
bool back(){
  if(g_input!=Input::none){clear_editor();return true;}
  if(g_view!=View::methods){g_view=View::methods;g_error[0]=0;return true;}
  return false;
}
} // namespace orcsdr::setup_ui
