from pathlib import Path
import subprocess
import shutil
root=Path(__file__).resolve().parents[1]
s=(root/'apps/orcsdr-tab5/ui/location_estimate.cpp').read_text(encoding='utf-8')
a=s.index('void service() {');b=s.index('\nbool request(bool',a)
assert 'vTaskDelete(nullptr)' not in s
assert 'g_finished.store(true,std::memory_order_release); vTaskSuspend(nullptr);' in s
assert '3,&g_task,1,' in s
h='''#include <atomic>
#include <cassert>
using TaskHandle_t=void*;TaskHandle_t g_task=nullptr;
std::atomic_bool g_busy{false},g_finished{false};
enum State{eRunning,eSuspended};State status=eRunning;int frees=0;
State eTaskGetState(TaskHandle_t){return status;}
void vTaskDeleteWithCaps(TaskHandle_t task){assert(task&&task==g_task);++frees;}
'''+s[a:b]+'''
int main(){service();assert(frees==0);g_task=(void*)1;g_busy=true;service();assert(frees==0&&g_busy);
g_finished=true;service();assert(frees==0&&g_busy);status=eSuspended;service();assert(frees==1&&!g_task&&!g_busy&&!g_finished);service();assert(frees==1);
for(int i=0;i<20;++i){g_task=(void*)1;g_busy=true;g_finished=true;service();}assert(frees==21&&!g_busy);}
'''
f=root/'.local/maps/location-task-lifetime';f.mkdir(parents=True,exist_ok=True);p=f/'test.cpp';p.write_text(h)
def linux(p):return str(p) if shutil.which('g++') else '/mnt/'+str(p)[0].lower()+str(p)[2:].replace('\\','/')
subprocess.run((['g++'] if shutil.which('g++') else ['wsl','g++'])+['-std=c++17','-Wall','-Wextra','-Werror',linux(p),'-o',linux(f/'test')],check=True)
subprocess.run(([] if shutil.which('g++') else ['wsl'])+[linux(f/'test')],check=True)
print('PASS: active task cannot be freed; suspended WithCaps task reclaimed once; 20 repeated completions release busy state without cleanup-task allocation')
