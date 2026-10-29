#pragma once
#include "nvs_store.hpp"
#include "keyboard_input.hpp"
#include "screen_controller.hpp"

namespace orcsdr::map_screen {
enum class Mode { picker, inventory, setup, downloads };
struct Hooks {
  void (*restore)(screens::Id) = nullptr;
  bool (*location)(int32_t latitude_e7,int32_t longitude_e7) = nullptr;
  bool (*pack)(const char* identity) = nullptr;
  void (*brand)() = nullptr;
  bool (*setup_reception)(bool pause) = nullptr;
};
// Call immediately after opening preferences, before legacy defaults are saved.
void load(NvsStore&);
bool first_run_pending();
void configure(const Hooks&);
bool open(Mode,bool automatic = false);
enum class Control {cancel,next,back,quick_start,finish,save,zoom_in,zoom_out,pan,set_proposal,select_pack};
struct State {
  bool active=false,proposal=false,editing=false;
  Mode mode=Mode::inventory;
  uint8_t step=0,zoom=0;
  int32_t latitude_e7=0,longitude_e7=0;
  bool downloads=false;
};
State state();
// UI-task only. Pan deltas move the camera east/south in screen pixels.
bool control(Control,int32_t first=0,int32_t second=0,const char* identity=nullptr);
void refresh();
void draw();
// Release setup reception ownership after external navigation, without painting.
// UI-task only; safe before screen-specific early returns and queued tuning.
void service_navigation();
void tick();
void touch(int x,int y,bool pressed,bool new_press);
void key(const keyboard_input::Key&);
} // namespace orcsdr::map_screen
