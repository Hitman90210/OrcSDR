from pathlib import Path
import subprocess
import shutil
root=Path(__file__).resolve().parents[1];s=(root/'apps/orcsdr-tab5/ui/adsb_dashboard.cpp').read_text(encoding='utf-8')
a=s.index('    const bool zoom_in=hit(');b=s.index('    for (size_t i = 0; i < g_aircraft_count;',a)
body=s[a:b]
h='''#include <cassert>
#include <cstdint>
#include <iterator>
constexpr int kZoomX=100,kZoomY=100,kZoomSize=50,kZoomStep=60;
constexpr uint16_t kRanges[]={10,25,50,100};
struct {uint16_t radar_range_nm=25;}g_settings;
enum class Action{none,settings_changed};int full=0,partial=0,invalidations=0,range_paints=0;
void redraw(){++full;}void draw_radar_map(){++partial;}void draw_radar_range(){++range_paints;}
bool hit(int x,int y,int a,int b,int w,int h){return x>=a&&x<a+w&&y>=b&&y<b+h;}
bool set_map_range(uint16_t range){g_settings.radar_range_nm=range;++invalidations;return true;}
'''
a=s.index('constexpr uint16_t zoomed_range(');b=s.index('\nstatic_assert',a);h+=s[a:b]+'''\nAction touch(int x,int y){\n'''+body+'''return Action::none;}
int main(){assert(touch(110,110)==Action::settings_changed);assert(g_settings.radar_range_nm==10&&partial==1&&full==0&&range_paints==1);
assert(touch(110,110)==Action::none);assert(partial==1&&invalidations==1);
g_settings.radar_range_nm=100;assert(touch(110,170)==Action::none);assert(partial==1&&invalidations==1);
assert(touch(110,110)==Action::settings_changed);assert(g_settings.radar_range_nm==50&&partial==2&&full==0&&range_paints==2);}
'''
f=root/'.local/maps/adsb-zoom-redraw';f.mkdir(parents=True,exist_ok=True);p=f/'test.cpp';p.write_text(h)
def linux(p):return str(p) if shutil.which('g++') else '/mnt/'+str(p)[0].lower()+str(p)[2:].replace('\\','/')
subprocess.run((['g++'] if shutil.which('g++') else ['wsl','g++'])+['-std=c++17','-Wall','-Wextra','-Werror',linux(p),'-o',linux(f/'test')],check=True);subprocess.run(([] if shutil.which('g++') else ['wsl'])+[linux(f/'test')],check=True)
print('PASS: zoom redraws only map; upper/lower range limits neither redraw nor report settings changes')
