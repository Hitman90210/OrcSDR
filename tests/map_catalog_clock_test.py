"""Exercise the production catalog clock wait with a deterministic UI/network clock."""
from pathlib import Path
import shutil
import subprocess

root = Path(__file__).resolve().parents[1]
source = (root / "apps/orcsdr-tab5/ui/map_downloads.cpp").read_text(encoding="utf-8")
start = source.index("bool ensure_catalog_clock() {")
end = source.index("\nvoid check(", start)
helper = source[start:end]
main = (root / "apps/orcsdr-tab5/ui/main.cpp").read_text(encoding="utf-8")
assert "maps::downloads::take_time_sync_request()" in main
assert source.index("if(!ensure_catalog_clock())return;") < source.index("catalog::fetch(stream")
harness = r'''
#include <atomic>
#include <cassert>
#include <cstdint>
#include <string>
#include <cstdio>
enum class Phase{checking,failed,cancelled};
std::atomic<bool> g_time_sync_requested{false};
uint64_t mono=0;bool valid=false,connected=true,stop=false;int success_at=-1,disconnect_at=-1,cancel_at=-1;
int sync_requests=0;std::string message;
namespace time_service {struct Snapshot{bool wallclock_valid;};Snapshot now(){return {valid};}}
namespace wifi {bool connected(){return ::connected;}}
uint64_t esp_timer_get_time(){return mono*1000;}
uint32_t pdMS_TO_TICKS(uint32_t x){return x;}
bool cancelled(void*){return stop;}
void publish(Phase,bool,const char* s){message=s;}
void finish(Phase,bool,const char* s){message=s;g_time_sync_requested=false;}
void vTaskDelay(uint32_t ms){
 mono+=ms;
 // Simulate the UI consuming the request, then completing SNTP/RTC publication.
 if(g_time_sync_requested.exchange(false))++sync_requests;
 if(success_at>=0&&mono>=static_cast<unsigned>(success_at))valid=true;
 if(disconnect_at>=0&&mono>=static_cast<unsigned>(disconnect_at))connected=false;
 if(cancel_at>=0&&mono>=static_cast<unsigned>(cancel_at))stop=true;
}
'''
harness += helper
harness += r'''
void reset(){mono=0;valid=false;connected=true;stop=false;success_at=disconnect_at=cancel_at=-1;sync_requests=0;g_time_sync_requested=false;message.clear();}
int main(){
 reset();valid=true;assert(ensure_catalog_clock()&&sync_requests==0&&mono==0);
 reset();success_at=300;assert(ensure_catalog_clock()&&sync_requests==1&&mono>=300);
 reset();assert(!ensure_catalog_clock()&&mono>=25000&&mono<=25100&&sync_requests==1);
 assert(message.find("Retry")!=std::string::npos);assert(!g_time_sync_requested);
 reset();cancel_at=200;assert(!ensure_catalog_clock()&&mono<1000&&message.find("cancelled")!=std::string::npos);
 reset();disconnect_at=200;assert(!ensure_catalog_clock()&&mono<1000&&message.find("Wi-Fi")!=std::string::npos);
 // A subsequent Check retries automatically after the network/time service recovers.
 reset();success_at=100;assert(ensure_catalog_clock()&&sync_requests==1);
 std::puts("PASS: catalog automatically waits for UI-owned time sync; timeout/cancel/disconnect bounded; retry succeeds");
}
'''
folder = root / ".local/maps/map-catalog-clock-test"
folder.mkdir(parents=True,exist_ok=True)
cpp, exe = folder / "test.cpp", folder / "test"
cpp.write_text(harness, encoding="utf-8")
def linux(path):
    return "/mnt/" + str(path)[0].lower() + str(path)[2:].replace("\\", "/")
if shutil.which("g++"):
    command = ["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror", str(cpp), "-o", str(exe)]
    run = [str(exe)]
else:
    command = ["wsl", "g++", "-std=c++17", "-Wall", "-Wextra", "-Werror", linux(cpp), "-o", linux(exe)]
    run = ["wsl", linux(exe)]
subprocess.run(command, check=True)
subprocess.run(run, check=True)
