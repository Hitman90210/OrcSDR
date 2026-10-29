from pathlib import Path
import subprocess,shutil
root=Path(__file__).resolve().parents[1];s=(root/'apps/orcsdr-tab5/ui/map_service.cpp').read_text(encoding='utf-8')
a=s.index('  g_lock=xSemaphoreCreateMutex();',s.index('bool begin('));b=s.index('  (void)memory_reclaim::register_holder',a)
h='''#include <cassert>
#include <cstdint>
using Handle=void*;Handle g_lock=nullptr,g_preview_lock=nullptr,g_discovery_lock=nullptr,g_queue=nullptr,g_task=nullptr,g_download_task=nullptr;
int fail=0,step=0,sems=0,queues=0,tasks=0,notices=0;constexpr int pdPASS=1,MALLOC_CAP_SPIRAM=1,MALLOC_CAP_8BIT=2;
struct Request{};Handle allocate(){++step;return step==fail?nullptr:reinterpret_cast<Handle>(static_cast<uintptr_t>(step));}
Handle xSemaphoreCreateMutex(){auto p=allocate();if(p)++sems;return p;}
Handle xQueueCreateWithCaps(int,unsigned,int){auto p=allocate();if(p)++queues;return p;}
void vSemaphoreDelete(Handle){--sems;}void vQueueDeleteWithCaps(Handle){--queues;}
void worker(void*){}void download_worker(void*){}
int xTaskCreatePinnedToCoreWithCaps(void(*)(void*),const char*,int,void*,int,Handle* p,int,int caps){assert(caps==3);*p=allocate();if(*p)++tasks;return *p?1:0;}
int xTaskCreatePinnedToCore(void(*)(void*),const char*,int,void*,int,Handle* p,int){*p=allocate();if(*p)++tasks;return *p?1:0;}
void vTaskDeleteWithCaps(Handle p){assert(p==g_download_task);--tasks;}void xTaskNotifyGive(Handle p){assert(p==g_download_task&&g_task);++notices;}
bool start(){
'''+s[a:b]+'''return true;}
int main(){for(fail=1;fail<=5;++fail){step=sems=queues=tasks=notices=0;assert(!start());assert(!g_lock&&!g_preview_lock&&!g_discovery_lock&&!g_queue&&!g_task&&!g_download_task);assert(!sems&&!queues&&!tasks&&!notices);}
fail=0;step=0;assert(start());assert(sems==3&&queues==1&&tasks==1&&notices==0);}
'''
f=root/'.local/maps/map-worker-startup';f.mkdir(parents=True,exist_ok=True);p=f/'test.cpp';p.write_text(h)
def linux(p):return str(p) if shutil.which('g++') else '/mnt/'+str(p)[0].lower()+str(p)[2:].replace('\\','/')
subprocess.run((['g++']if shutil.which('g++')else['wsl','g++'])+['-std=c++17','-Wall','-Wextra','-Werror',linux(p),'-o',linux(f/'test')],check=True);subprocess.run(([]if shutil.which('g++')else['wsl'])+[linux(f/'test')],check=True)
print('PASS: five renderer startup allocation/task failures release every owned resource; download allocation is separate from permanent renderer startup')
