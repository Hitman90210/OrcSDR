from pathlib import Path
import subprocess
import shutil
root=Path(__file__).resolve().parents[1]
source=(root/'apps/orcsdr-tab5/ui/map_screen.cpp').read_text(encoding='utf-8')
main=(root/'apps/orcsdr-tab5/ui/main.cpp').read_text(encoding='utf-8')
start=source.index('void service_navigation() {')
end=source.index('\nvoid tick()',start)
body=source[start:end]
harness='''#include <cassert>
namespace screens {enum class Id{maps,home,documentation,rf_lab};Id current=Id::maps;bool is_active(Id id){return current==id;}}
namespace setup_ui {int resets=0;void reset(){++resets;}}
bool g_setup_paused=false;int resumes=0;
bool resume(bool pause){assert(!pause);++resumes;return true;}
struct {bool(*setup_reception)(bool)=resume;}g_hooks;
'''+body+'''
int main(){g_setup_paused=true;service_navigation();assert(g_setup_paused&&resumes==0);
for(auto id:{screens::Id::home,screens::Id::documentation,screens::Id::rf_lab}){
g_setup_paused=true;screens::current=id;service_navigation();assert(!g_setup_paused);int before=resumes;service_navigation();assert(resumes==before);}
assert(resumes==3&&setup_ui::resets==3);}
'''
harness='#include <initializer_list>\n'+harness
queue=main[main.index('bool queue_local_rtl_listen(RtlBand band, uint32_t frequency_hz,',main.index('bool queue_local_rtl_listen(RtlBand band, uint32_t frequency_hz,')+1):]
queue=queue[:queue.index('\nvoid adjust_rtl_volume')]
assert 'if(setup_receiver_inhibit.load(std::memory_order_acquire))return false;' not in queue
assert 'orcsdr::map_screen::service_navigation();' not in queue
doc=main[main.index('void ui_doc_enter() {'):main.index('bool ui_doc_pause_reception()')]
assert doc.index('screens::finish_transition();')<doc.index('map_screen::service_navigation();')
assert 'if (setup_radio_paused || catalog_radio_paused' in queue
loop=main[main.index('void loop()'):]
assert loop.index('orcsdr::map_screen::service_navigation();')<loop.index('if (orcsdr::visualizer::active())')
folder=root/'.local/maps/setup-navigation';folder.mkdir(parents=True,exist_ok=True)
cpp=folder/'test.cpp';cpp.write_text(harness,encoding='utf-8')
def linux(p):return str(p) if shutil.which('g++') else '/mnt/'+str(p)[0].lower()+str(p)[2:].replace('\\','/')
subprocess.run((['g++'] if shutil.which('g++') else ['wsl','g++'])+['-std=c++17','-Wall','-Wextra','-Werror',linux(cpp),'-o',linux(folder/'test')],check=True)
subprocess.run(([] if shutil.which('g++') else ['wsl'])+[linux(folder/'test')],check=True)
print('PASS: setup remains paused while owned; Home/documentation/RF Lab release once; queued navigation defers reception; cleanup precedes early returns')
