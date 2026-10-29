"""Fault-inject the production logging task and real record serializers."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
HARNESS = r'''
#include "orcsdr_storage.hpp"
#include "lora_packet_log.hpp"
#include "lora_history.hpp"
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <map>
#include <string>
#include <vector>
using LoraDisplayPacket=orcsdr::lora_log::Record;
std::string mode;
std::map<std::string,std::string> disk;
bool fired=false,rollback_fired=false;unsigned opens=0,receives=0;
uint64_t elapsed=0;uint32_t origin=0;
struct End {};
std::atomic<bool> lora_log_requested{true},lora_log_ready{false},lora_log_error{false};
std::atomic<bool> lora_log_pending{false},lora_log_retry_requested{false};
std::atomic<uint32_t> lora_log_retry_count{0},lora_log_dropped{0},lora_log_last_packet_ms{0};
std::atomic<const char*> lora_log_error_stage{"none"};
std::atomic<int> lora_log_error_errno{0};
std::atomic<bool> lora_history_loaded{false};
std::atomic<uint32_t> lora_history_replayed{0},lora_history_rejected{0},lora_history_written{0},lora_history_revision{0};
orcsdr::lora_history::Store lora_history;
constexpr char kLoraLogPath[]="/orcsdr/lora_packets_v2.csv";
constexpr uint32_t kLoraLogRetryMs=2000;
std::deque<LoraDisplayPacket> queue;
void* lora_log_queue=reinterpret_cast<void*>(1);
#define EXT_RAM_BSS_ATTR
#define portENTER_CRITICAL(x) ((void)0)
#define portEXIT_CRITICAL(x) ((void)0)
#define pdTRUE 1
#define pdMS_TO_TICKS(x) (x)
uint32_t millis(){return origin+uint32_t(elapsed);}
void advance(unsigned n){elapsed+=n;if(mode=="off_resume"&&elapsed>=4000){lora_log_requested=true;lora_log_retry_requested=true;}if(elapsed>=12000)throw End{};}
void vTaskDelay(unsigned n){advance(n);}
int xQueueReceive(void*,void* out,unsigned wait){
 ++receives;
 if(queue.empty()){advance(wait);return 0;}
 *static_cast<LoraDisplayPacket*>(out)=queue.front();queue.pop_front();return 1;
}
unsigned uxQueueMessagesWaiting(void*){return unsigned(queue.size());}
int xQueueSend(void*,const void* in,unsigned){
 if(queue.size()==32)return 0;
 queue.push_back(*static_cast<const LoraDisplayPacket*>(in));return 1;
}
struct SerialStub {
 void println(const char*){}
 void printf(const char* format,...){char out[512];va_list a;va_start(a,format);vsnprintf(out,sizeof(out),format,a);va_end(a);}
} Serial;
bool ensure_tab5_sd(){return true;}
bool lora_archive_name(const std::string& name){return name.size()>6&&name.substr(name.size()-6)==".jsonl";}
namespace orcsdr::storage {
struct File::State {std::string path;size_t at=0,next=0;bool directory=false;std::vector<std::string> entries;};
File::File(std::shared_ptr<State> p):state_(std::move(p)){}
File::operator bool()const{return bool(state_);}
bool File::isDirectory()const{return state_&&state_->directory;}
const char* File::name()const{return state_?state_->path.c_str():"";}
File File::openNextFile(){
 if(!state_||state_->next==state_->entries.size())return {};
 return FileSystem{}.open(state_->entries[state_->next++].c_str());
}
size_t File::size()const{return state_&&!state_->directory?disk[state_->path].size():0;}
size_t File::position()const{return state_?state_->at:0;}
bool File::seek(size_t n){if(!state_||n>size()){errno=EIO;return false;}state_->at=n;return true;}
bool File::available()const{return state_&&state_->at<size();}
size_t File::read(uint8_t* out,size_t n){n=std::min(n,size()-state_->at);memcpy(out,disk[state_->path].data()+state_->at,n);state_->at+=n;return n;}
bool File::flush(){return bool(state_);}
bool File::sync(){
 bool journal=state_&&state_->path.find(".jsonl")!=std::string::npos;
 if(!fired&&((mode=="sync_journal"&&journal)||(mode=="sync_csv"&&!journal)||
    (mode=="later_sync"&&!journal&&disk[state_->path].find("queued-2")!=std::string::npos))){fired=true;errno=EIO;return false;}
 return bool(state_);
}
bool File::truncate(size_t n){if(mode=="rollback"&&!rollback_fired){rollback_fired=true;errno=EIO;return false;}if(!state_||n>size()){errno=EIO;return false;}disk[state_->path].resize(n);state_->at=n;return true;}
bool File::close(){state_.reset();return true;}
size_t File::write(const uint8_t* in,size_t n){
 assert(state_&&!state_->directory);
 bool journal=state_->path.find(".jsonl")!=std::string::npos;
 bool fail=!fired&&((mode=="journal_write"&&journal)||(mode=="wrap"&&journal)||
   ((mode=="csv_write"||mode=="header"||mode=="bounds"||mode=="rollback")&&!journal)||
   (mode=="later_csv"&&!journal&&std::string(reinterpret_cast<const char*>(in),n).find("queued-2")!=std::string::npos)||
   (mode=="later_journal"&&journal&&std::string(reinterpret_cast<const char*>(in),n).find("\"id\":2,")!=std::string::npos));
 size_t actual=fail?n/2:n;
 auto& data=disk[state_->path];
 if(state_->at+actual>data.size())data.resize(state_->at+actual);
 memcpy(data.data()+state_->at,in,actual);state_->at+=actual;
 if(fail){fired=true;errno=ENOSPC;if(mode=="bounds")data+=std::string(2048,'X');}
 return actual;
}
size_t File::print(const char* s){return write(reinterpret_cast<const uint8_t*>(s),strlen(s));}
File FileSystem::open(const char* name,const char* how,bool)const{
 ++opens;std::string path(name);bool journal=path.find(".jsonl")!=std::string::npos;
 if((!fired&&((mode=="open_csv"&&path==kLoraLogPath)||(mode=="open_journal"&&journal)))||
    ((mode=="permanent"||mode=="off"||(mode=="off_resume"&&!fired))&&journal)||(mode=="queuefull"&&path==kLoraLogPath)){
  fired=true;errno=EMFILE;if(mode=="off"||mode=="off_resume")lora_log_requested=false;return {};
 }
 auto p=std::make_shared<File::State>();p->path=path;
 if(path=="/orcsdr/lora"){
  p->directory=true;for(const auto& item:disk)if(item.first.find(path+"/")==0)p->entries.push_back(item.first);
 }else{
  if(how[0]=='w')disk[path].clear();
  else if(!disk.count(path)){if(how[0]=='a')disk[path]="";else{errno=ENOENT;return {};}}
  if(how[0]=='a')p->at=disk[path].size();
 }
 return File{std::move(p)};
}
bool FileSystem::exists(const char* name)const{return std::string(name)=="/orcsdr"||std::string(name)=="/orcsdr/lora"||disk.count(name);}
EntryStatus FileSystem::status(const char* name)const{if(mode=="stat"&&!fired&&std::string(name)==kLoraLogPath){fired=true;errno=EIO;return EntryStatus::error;}return exists(name)?(disk.count(name)?EntryStatus::file:EntryStatus::directory):EntryStatus::missing;}
bool FileSystem::mkdir(const char*)const{return true;}
}
orcsdr::storage::FileSystem fs;
auto* g_sd_fs=&fs;
/* ERROR_HELPER */
/* TASK */
/* ENQUEUE */
int main(int argc,char** argv){
 assert(argc==2);mode=argv[1];if(mode=="wrap")origin=UINT32_MAX-500;
 const std::string prefix=mode=="header"?"":"existing CSV\n";
 if(mode!="header")disk[kLoraLogPath]=prefix;
 for(unsigned id=1;id<=unsigned(mode=="queuefull"?32:2);++id){
  LoraDisplayPacket r{};r.sender=0x12345678;r.packet_id=id;r.port=1;r.received_utc=1793000000;
  snprintf(r.text,sizeof(r.text),"queued-%u",id);queue.push_back(r);
 }
 if(mode=="queuefull"){LoraDisplayPacket r{};enqueue_lora_sd_log(r);}
 try{lora_sd_log_task(nullptr);}catch(const End&){}
 if(mode=="permanent"||mode=="off"||mode=="bounds"){
  assert(lora_log_dropped==0&&queue.size()==1&&lora_log_pending&&lora_history_written==0);
  assert(lora_log_error);assert(lora_log_retry_count<=6);assert(opens<30);
  if(mode=="off")assert(lora_log_retry_count==0);
  if(mode=="bounds")assert(disk[kLoraLogPath].find(std::string(2048,'X'))!=std::string::npos);
 }else if(mode=="queuefull"){
  assert(lora_log_dropped==1&&queue.size()==32&&!lora_log_pending&&lora_history_written==0);
 }else{
  assert(lora_log_dropped==0&&queue.empty()&&!lora_log_pending&&lora_history_written==2);
  assert(lora_log_ready&&!lora_log_error);
  const auto& csv=disk[kLoraLogPath];assert(csv.find(prefix)==0);
  for(unsigned id=1;id<=2;++id){auto needle="queued-"+std::to_string(id);auto at=csv.find(needle);assert(at!=std::string::npos&&csv.find(needle,at+1)==std::string::npos);}
  std::vector<uint32_t> ids;
  for(const auto& item:disk)if(item.first.find(".jsonl")!=std::string::npos){
   size_t at=0;while(at<item.second.size()){
    auto end=item.second.find('\n',at);assert(end!=std::string::npos);LoraDisplayPacket r{};
    assert(orcsdr::lora_history::parse_line(item.second.data()+at,end-at,&r));ids.push_back(r.packet_id);at=end+1;
   }
  }
  assert((ids==std::vector<uint32_t>{1,2}));
  if(mode!="normal")assert(lora_log_retry_count>=1);
 }
 puts(("LORA_LOG_RECOVERY_PASS "+mode).c_str());
}
'''

source=(ROOT/'apps/orcsdr-tab5/ui/main.cpp').read_text(encoding='utf-8')
start=source.index('void lora_sd_log_task(void*) {')
end=source.index('\nvoid set_lora_sd_logging(',start)
task=source[start:end]
helper=''
if 'void lora_sd_log_failure(' in source:
    helper=source[source.index('void lora_sd_log_failure('):start]
start=source.index('void enqueue_lora_sd_log(const LoraDisplayPacket& packet) {')
end=source.index('\nstatic void write_le16',start)
harness=HARNESS.replace('/* TASK */',task).replace('/* ERROR_HELPER */',helper).replace('/* ENQUEUE */',source[start:end])
with tempfile.TemporaryDirectory(prefix='lora-recovery-') as directory:
    path=Path(directory);(path/'test.cpp').write_text(harness)
    ui=ROOT/'apps/orcsdr-tab5/ui'
    subprocess.run(['g++','-std=c++17','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-sanitize-recover=all','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-I'+str(ui),str(path/'test.cpp'),str(ui/'lora_packet_log.cpp'),str(ui/'lora_history.cpp'),'-o',str(path/'test')],check=True)
    for mode in ('journal_write','normal','csv_write','header','sync_journal','sync_csv','open_csv','open_journal','permanent','off','bounds','queuefull','wrap','rollback','stat','off_resume','later_csv','later_journal','later_sync'):
        subprocess.run([str(path/'test'),mode],check=True)
