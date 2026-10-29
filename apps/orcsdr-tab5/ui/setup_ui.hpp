#pragma once
#include "keyboard_input.hpp"
#include <cstdint>

namespace orcsdr::setup_ui {
struct Network {char ssid[33]{};bool secure=false;};
struct Snapshot {
  bool connected=false,connecting=false,scanning=false,location_set=false;
  char ssid[33]{},message[80]{};
  Network networks[6]{};
  uint8_t count=0;
  int32_t latitude_e7=0,longitude_e7=0;
  bool lookup_busy=false,lookup_ready=false;
  int32_t lookup_latitude_e7=0,lookup_longitude_e7=0;
  char lookup_label[40]{},lookup_message[80]{};
};
struct Hooks {
  void (*snapshot)(Snapshot*)=nullptr;
  void (*scan)()=nullptr;
  bool (*connect)(const char*,const char*)=nullptr;
  bool (*lookup)(const char*)=nullptr; // nullptr requests approximate network area
  bool (*save)(int32_t,int32_t)=nullptr;
};
enum class Page {network,location};
enum class Result {none,redraw,picker};
void configure(const Hooks&);
void reset();
void draw(Page);
bool poll();
Result touch(Page,int x,int y);
Result key(Page,const keyboard_input::Key&);
bool input_active();
bool back(); // true consumes Back inside an input or location subview
} // namespace orcsdr::setup_ui
