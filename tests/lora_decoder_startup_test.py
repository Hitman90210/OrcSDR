"""Fault-inject the production LoRa task startup without hardware or RF data."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / "apps/orcsdr-tab5/ui/main.cpp").read_text(encoding="utf-8")
start = source.index("bool lora_native_decoder_start() {")
end = source.index("\nvoid lora_iq_offer(", start)
harness = r'''
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdio>
using QueueHandle_t = void*;
using TaskHandle_t = void*;
constexpr int pdPASS=1, MALLOC_CAP_SPIRAM=1, MALLOC_CAP_8BIT=2;
struct LoraNativeDecodeWork { int value; };
QueueHandle_t lora_native_decode_queue=nullptr;
TaskHandle_t lora_native_decode_task_handle=nullptr;
std::atomic<bool> lora_native_decoder_ready{false};
bool fail_queue=false, fail_task=false, fail_initialize=false, fail_workspace=false;
int queues=0, tasks=0, deletes=0;
struct { void println(const char*) {} } Serial;
namespace orcsdr::lora_native {
bool self_check(){return true;}
bool initialize(){return !fail_initialize;}
void* task_packets(){return fail_workspace ? nullptr : reinterpret_cast<void*>(1);}
void* task_stats(){return task_packets();}
}
void log_lora_memory(const char*){}
void lora_native_decode_task(void*){}
QueueHandle_t xQueueCreate(int n, size_t size) {
 assert(n==1 && size==sizeof(LoraNativeDecodeWork));
 if(fail_queue)return nullptr;
 ++queues;return reinterpret_cast<void*>(1);
}
void vQueueDelete(QueueHandle_t q){assert(q);--queues;++deletes;}
// A 12 KiB internal stack cannot fit the observed 7 KiB largest block.
int xTaskCreatePinnedToCore(void(*)(void*),const char*,int,void*,int,TaskHandle_t*,int){return 0;}
int xTaskCreatePinnedToCoreWithCaps(void(*)(void*),const char*,int bytes,void*,int priority,TaskHandle_t* out,int core,int caps){
 assert(bytes==12288 && priority==1 && core==0 && caps==(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
 if(fail_task)return 0;
 ++tasks;*out=reinterpret_cast<void*>(2);return pdPASS;
}
'''
harness += source[start:end]
harness += r'''
int main(){
 fail_initialize=true;assert(!lora_native_decoder_start() && queues==0);fail_initialize=false;
 fail_workspace=true;assert(!lora_native_decoder_start() && queues==0);fail_workspace=false;
 fail_queue=true;assert(!lora_native_decoder_start() && queues==0 && !lora_native_decode_queue);fail_queue=false;
 fail_task=true;assert(!lora_native_decoder_start());
 assert(!lora_native_decoder_ready && !lora_native_decode_queue && !lora_native_decode_task_handle && queues==0 && deletes==1);
 fail_task=false;assert(lora_native_decoder_start());
 assert(lora_native_decoder_ready && lora_native_decode_queue && lora_native_decode_task_handle && queues==1 && tasks==1);
 assert(lora_native_decoder_start() && queues==1 && tasks==1);
 std::puts("LORA_DECODER_STARTUP_PASS: failures clean up, retry works, PSRAM stack, idempotent startup");
}
'''
with tempfile.TemporaryDirectory(prefix="orcsdr-lora-startup-") as temp:
    cpp = Path(temp) / "startup.cpp"
    exe = Path(temp) / "startup"
    cpp.write_text(harness, encoding="utf-8")
    subprocess.run(["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror", str(cpp), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
