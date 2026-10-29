from pathlib import Path
import subprocess
import shutil
root=Path(__file__).resolve().parents[1];s=(root/'apps/orcsdr-tab5/ui/main.cpp').read_text(encoding='utf-8')
a=s.index('    if(!orcdial_inbox)orcdial_inbox=xQueueCreateWithCaps');b=s.index('    if(orcdial_inbox&&orcdial_tx_task&&eh_host_feat_peer_data_init()',a)
body=s[a:b]
h='''#include <cstdint>
#include <cassert>
using Handle=void*;Handle orcdial_inbox=nullptr,orcdial_outbox=nullptr,orcdial_tx_task=nullptr;
struct OrcDialFrame{};constexpr int MALLOC_CAP_SPIRAM=1,MALLOC_CAP_8BIT=2,MALLOC_CAP_INTERNAL=4,MALLOC_CAP_RTCRAM=8,pdPASS=1;
int fail=0;uint32_t initialization_retry_ms=0;uint32_t millis(){return 100;}
void orcdial_transmit(void*){}void log_dram_budget(const char*){}
Handle xQueueCreateWithCaps(int count,unsigned,int){return fail==count?nullptr:(void*)1;}
int xTaskCreatePinnedToCoreWithCaps(void(*)(void*),const char*,int,void*,int,Handle* out,int,int){if(fail==3)return 0;*out=(void*)1;return pdPASS;}
bool attempt(){
'''+body+'''return true;}
int main(){for(int missing:{20,16,3}){fail=missing;orcdial_inbox=orcdial_outbox=orcdial_tx_task=nullptr;initialization_retry_ms=0;assert(!attempt());assert(initialization_retry_ms==5100);}
fail=0;orcdial_inbox=orcdial_outbox=orcdial_tx_task=nullptr;assert(attempt());assert(orcdial_inbox&&orcdial_outbox&&orcdial_tx_task);}
'''
# Production is void; return from an allocation failure is the observed early exit.
h=h.replace('initialization_retry_ms=millis()+5000;return;','initialization_retry_ms=millis()+5000;return false;')
h='#include <initializer_list>\n'+h
f=root/'.local/maps/orcdial-init-backoff';f.mkdir(parents=True,exist_ok=True);p=f/'test.cpp';p.write_text(h)
def linux(p):return str(p) if shutil.which('g++') else '/mnt/'+str(p)[0].lower()+str(p)[2:].replace('\\','/')
subprocess.run((['g++'] if shutil.which('g++') else ['wsl','g++'])+['-std=c++17','-Wall','-Wextra','-Werror',linux(p),'-o',linux(f/'test')],check=True);subprocess.run(([] if shutil.which('g++') else ['wsl'])+[linux(f/'test')],check=True)
print('PASS: inbox/outbox/task allocation failures defer initialization five seconds; successful resources proceed')
