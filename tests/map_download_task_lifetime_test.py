from pathlib import Path
import subprocess,shutil
root=Path(__file__).resolve().parents[1];s=(root/'apps/orcsdr-tab5/ui/map_service.cpp').read_text(encoding='utf-8')
a=s.index('void service_downloads() {');b=s.index('\nbool running()',a);body=s[a:b]
assert 'MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT' in body and 'MALLOC_CAP_SPIRAM' not in body
h='''#include <atomic>
#include <cstdio>
#include <cassert>
using Handle=void*;Handle g_task=nullptr,g_download_task=nullptr;std::atomic_bool g_download_finished{false};
enum eTaskState{eRunning,eSuspended};eTaskState status=eRunning;int attempts=0,frees=0,failures=0;bool fail=false,pending=false;
constexpr int pdPASS=1,MALLOC_CAP_INTERNAL=1,MALLOC_CAP_8BIT=2;
namespace downloads {bool work_pending(){return pending;}void worker_unavailable(){++failures;pending=false;}}
eTaskState eTaskGetState(Handle){return status;}
unsigned uxTaskGetStackHighWaterMark(Handle){return 4096;}
void vTaskDeleteWithCaps(Handle h){assert(h==g_download_task&&status==eSuspended);++frees;}
void download_worker(void*){}
int xTaskCreatePinnedToCoreWithCaps(void(*)(void*),const char*,int bytes,void*,int,Handle* out,int,int caps){assert(bytes==8192&&caps==3);++attempts;if(fail)return 0;*out=(void*)2;pending=false;return 1;}
'''+body+'''
int main(){service_downloads();assert(attempts==0);g_task=(void*)1;service_downloads();assert(attempts==0);
pending=true;fail=true;service_downloads();assert(attempts==1&&failures==1&&!g_download_task);
pending=true;fail=false;service_downloads();assert(attempts==2&&g_download_task);service_downloads();assert(attempts==2);
g_download_finished=true;service_downloads();assert(frees==0);status=eSuspended;service_downloads();assert(frees==1&&!g_download_task&&!g_download_finished);service_downloads();assert(frees==1&&attempts==2);
}
'''
f=root/'.local/maps/map-download-task-lifetime';f.mkdir(parents=True,exist_ok=True);p=f/'test.cpp';p.write_text(h)
def linux(p):return str(p)if shutil.which('g++')else'/mnt/'+str(p)[0].lower()+str(p)[2:].replace('\\','/')
subprocess.run((['g++']if shutil.which('g++')else['wsl','g++'])+['-std=c++17','-Wall','-Wextra','-Werror',linux(p),'-o',linux(f/'test')],check=True);subprocess.run(([]if shutil.which('g++')else['wsl'])+[linux(f/'test')],check=True)
print('PASS: download/NVS stack internal, allocated only for queued work, failures exposed, suspended job reaped once, idle memory restored')
