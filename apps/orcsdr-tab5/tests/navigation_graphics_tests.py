"""Run actual navigation functions against fake graphics and screen hooks.

Run under Linux/WSL with g++. No firmware build or device operations.
"""
from pathlib import Path
import subprocess
import tempfile

source = (Path(__file__).resolve().parents[1] / "ui/navigation_service.cpp").read_text()


def function(signature):
    start = source.index(signature + " {")
    end = source.index("{", start) + 1
    depth = 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


code = r'''
#include <cassert>
#include <cmath>
#include <iterator>
#include <cstdio>
#include "screen_controller.hpp"
namespace screens = orcsdr::screens;
using std::size_t;
constexpr int TFT_BLACK=0;
unsigned millis(){return 0;}
struct {struct {void fillScreen(int){}} Display;} M5;
namespace settings {
enum class Section {network,location};
bool enabled=false;Section selected=Section::network;
bool active(){return enabled;}
void enter(int,Section section){enabled=true;selected=section;}
void leave(){enabled=false;}
}
namespace home {void enter(int){}void draw_spectrum(float*,int,size_t,float){}}
namespace fm {void leave(){}}
namespace am {void leave(){}}
namespace cb {void leave(){}}
namespace airband {void leave(){}}
namespace p25 {void leave(){}}
namespace pocsag {void leave(){}}
namespace adsb {void leave(){}}
namespace lora {void leave(){}}
namespace rf24 {void leave(){}}
namespace ft8 {void leave(){}}
namespace sstv {void stop(){}void ui_leave(){}}
bool graphics=true,g_restore_graphics=true;int disables=0;
bool disable(){++disables;bool old=graphics;graphics=false;return old;}
void restore(bool old){graphics=old;}
struct {
  void (*persist_settings_open)()=nullptr;
  bool (*disable_graphics)()=disable;
  void (*restore_graphics)(bool)=restore;
  void (*close_overlays)()=+[]{};
  int (*settings_state)()=+[]{return 0;};
  void (*restore_screen)(screens::Id)=+[](screens::Id){};
  void (*sync_audio)()=+[]{};
  bool (*audio_enabled)()=+[]{return false;};
  bool (*ensure_speaker_running)(int)=+[](int){return true;};
  int (*volume)()=+[]{return 0;};
  int (*home_snapshot)(bool)=+[](bool){return 0;};
} g_hooks;
bool ready(){return true;}
'''
code += "\n".join(function(s) for s in (
    "void open_settings(settings::Section section)", "void close_settings()",
    "void show_home(bool demo)"))
code += r'''
int main(){
  for(bool initially_on:{true,false})for(bool via_home:{false,true}){
    graphics=initially_on;disables=0;settings::enabled=false;
    screens::begin_transition(screens::Id::home,0);screens::finish_transition();
    open_settings(settings::Section::network);
    assert(!graphics&&settings::active());
    open_settings(settings::Section::location);
    assert(!graphics&&settings::selected==settings::Section::location);
    if(via_home)show_home(false);else close_settings();
    assert(graphics==initially_on);
    assert(disables==1&&!settings::active()&&screens::status().active==screens::Id::home);
  }
  std::puts("NAVIGATION_GRAPHICS_PASS repeated Settings, Close/Home, enabled/disabled");
}
'''
with tempfile.TemporaryDirectory() as directory:
    cpp = Path(directory) / "test.cpp"
    binary = Path(directory) / "test"
    cpp.write_text(code)
    ui = Path(__file__).resolve().parents[1] / "ui"
    subprocess.run(["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror", "-I", str(ui),
                    str(cpp), str(ui / "screen_controller.cpp"), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
