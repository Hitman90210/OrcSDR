"""Exercise the production automatic-capture queue handoff, including startup failure."""
from pathlib import Path
import subprocess
import shutil

root=Path(__file__).resolve().parents[1]
source=(root/'apps/orcsdr-tab5/ui/main.cpp').read_text(encoding='utf-8')
start=source.index('bool queue_lora_auto_decode() {')
end=source.index('\nvoid iq_rec_finish()',start)
harness=r'''
#include <atomic>
#include <cstdint>
#include <cstddef>
#include <cassert>
#include <cstdio>
struct {template<class... T>void printf(const char*,T...) {}} Serial;
uint8_t first[16],second[16];uint8_t* g_iq_rec_buf=first;uint8_t* g_lora_decode_buf=second;
std::atomic<bool> lora_native_decoder_ready{false},g_iq_rec_ready{false},g_iq_rec_auto_triggered{false},lora_native_decode_busy{false};
std::atomic<size_t> g_iq_rec_write{0};std::atomic<uint32_t> lora_capture_drops{0},lora_native_failures{0};
void* lora_native_decode_queue=nullptr;
uint32_t g_iq_rec_sf=11,g_iq_rec_bandwidth_hz=250000,g_iq_rec_frequency_hz=906875000,g_iq_rec_sequence=7,g_iq_rec_started_ms=1,g_iq_rec_trigger_offset_samples=0;
struct LoraNativeDecodeWork {uint8_t* iq;size_t bytes;uint32_t sf,bw,hz;bool automatic;uint32_t sequence,started,queued,offset;};
uint32_t millis(){return 100;}
constexpr int pdTRUE=1;bool send_ok=true;int sends=0;
int xQueueSend(void*,const LoraNativeDecodeWork*,int){++sends;return send_ok?pdTRUE:0;}
'''+source[start:end]+r'''
void reset(){g_iq_rec_buf=first;g_lora_decode_buf=second;lora_native_decoder_ready=false;g_iq_rec_ready=true;g_iq_rec_auto_triggered=true;lora_native_decode_busy=false;g_iq_rec_write=16;lora_native_decode_queue=reinterpret_cast<void*>(1);lora_capture_drops=0;lora_native_failures=0;send_ok=true;sends=0;}
int main(){
 reset();assert(!queue_lora_auto_decode());
 assert(!g_iq_rec_ready&&!g_iq_rec_auto_triggered&&g_iq_rec_write==0&&lora_capture_drops==1&&sends==0);
 // The next detection can be delivered after startup recovers, without export/reset.
 lora_native_decoder_ready=true;g_iq_rec_ready=true;g_iq_rec_auto_triggered=true;g_iq_rec_write=16;
 assert(queue_lora_auto_decode()&&sends==1&&lora_native_decode_busy&&g_lora_decode_buf==first);
 reset();g_iq_rec_auto_triggered=false;assert(!queue_lora_auto_decode()&&g_iq_rec_ready&&g_iq_rec_write==16&&lora_capture_drops==0);
 reset();lora_native_decoder_ready=true;lora_native_decode_busy=true;
 assert(!queue_lora_auto_decode()&&g_iq_rec_ready&&g_iq_rec_auto_triggered&&g_iq_rec_write==16&&lora_capture_drops==0);
 reset();lora_native_decoder_ready=true;lora_native_decode_queue=nullptr;assert(!queue_lora_auto_decode()&&!g_iq_rec_ready&&lora_capture_drops==1);
 reset();lora_native_decoder_ready=true;g_lora_decode_buf=nullptr;assert(!queue_lora_auto_decode()&&!g_iq_rec_ready&&lora_capture_drops==1);
 reset();lora_native_decoder_ready=true;send_ok=false;assert(!queue_lora_auto_decode()&&!g_iq_rec_ready&&!lora_native_decode_busy&&lora_capture_drops==1&&lora_native_failures==1);
 std::puts("PASS: failed-start automatic slots released; manual captures and busy-decoder retry preserved; queue failure counted once");
}
'''
folder=root/'.local/maps/lora-auto-handoff';folder.mkdir(parents=True,exist_ok=True)
cpp,exe=folder/'test.cpp',folder/'test';cpp.write_text(harness,encoding='utf-8')
def linux(p):return '/mnt/'+str(p)[0].lower()+str(p)[2:].replace('\\','/')
if shutil.which('g++'):
 command=['g++','-std=c++17','-Wall','-Wextra','-Werror',str(cpp),'-o',str(exe)];run=[str(exe)]
else:
 command=['wsl','g++','-std=c++17','-Wall','-Wextra','-Werror',linux(cpp),'-o',linux(exe)];run=['wsl',linux(exe)]
subprocess.run(command,check=True);subprocess.run(run,check=True)
