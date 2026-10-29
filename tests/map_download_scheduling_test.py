from pathlib import Path
import subprocess,shutil
root=Path(__file__).resolve().parents[1];s=(root/'apps/orcsdr-tab5/ui/map_service.cpp').read_text(encoding='utf-8')
a=s.index('struct DiscoveryLock {');b=s.index('\ntemplate<size_t N>',a);lock=s[a:b]
a=s.index('void download_worker(void*) {');b=s.index('\nvoid worker(void*)',a);download=s[a:b]
a=s.index('void worker(void*) {');b=s.index('    // The UI never waits',a);render=s[a:b]
assert 'downloads::service' not in render
# Run the actual render scheduling prefix once, replacing only the renderer body.
render += '    ++renders;return; }catch(...){throw;} } }\n'
h='''#include <atomic>
#include <cassert>
#include <chrono>
#include <future>
#include <mutex>
#include <thread>
using TickType_t=int;constexpr int pdTRUE=1,portMAX_DELAY=-1;int pdMS_TO_TICKS(int n){return n;}
std::mutex mutex;void* g_discovery_lock=&mutex;int xSemaphoreTake(void*,int timeout){if(timeout==0)return mutex.try_lock()?1:0;mutex.lock();return 1;}
void xSemaphoreGive(void*){mutex.unlock();}
std::atomic_bool g_scan{true},g_download_finished{false};std::atomic<int> g_serial{1};int g_storage=0,g_queue=0,scans=0,renders=0;
struct Request{int serial=1;};int xQueueReceive(int,Request* r,int){r->serial=1;return 1;}
void discover(){++scans;}
struct Stop{};void vTaskDelay(int){throw Stop{};}void vTaskSuspend(void*){throw Stop{};}void ulTaskNotifyTake(int,int){}
std::promise<void> entered,release_job;auto done=release_job.get_future();
namespace downloads {bool service(int){entered.set_value();done.wait();return true;}}
'''+lock+download+render+'''
int main(){std::thread task([]{try{download_worker(nullptr);}catch(Stop&) {}});entered.get_future().wait();
auto rendering=std::async(std::launch::async,[]{worker(nullptr);});
assert(rendering.wait_for(std::chrono::seconds(2))==std::future_status::ready);rendering.get();
assert(renders==1&&scans==0&&g_scan);release_job.set_value();task.join();worker(nullptr);assert(renders==2&&scans==1&&!g_scan);
{DiscoveryLock lock(0);assert(lock.held);}assert(mutex.try_lock());mutex.unlock();}
'''
f=root/'.local/maps/map-download-scheduling';f.mkdir(parents=True,exist_ok=True);p=f/'test.cpp';p.write_text(h)
def linux(p):return str(p) if shutil.which('g++') else '/mnt/'+str(p)[0].lower()+str(p)[2:].replace('\\','/')
subprocess.run((['g++']if shutil.which('g++')else['wsl','g++'])+['-std=c++17','-pthread','-Wall','-Wextra','-Werror',linux(p),'-o',linux(f/'test')],check=True);subprocess.run(([]if shutil.which('g++')else['wsl'])+[linux(f/'test')],check=True)
print('PASS: blocked download does not block queued render; discovery/recovery deferred until transfer releases ownership; scoped ownership released')
