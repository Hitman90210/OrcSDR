"""Fault-inject the production staged FT8 log flush without decoder fixtures."""
from pathlib import Path
import shutil
import subprocess

root = Path(__file__).resolve().parents[1]
source = (root / "apps/orcsdr-tab5/ui/main.cpp").read_text(encoding="utf-8")
start = source.index("static bool ft8_log_flush(bool force) {")
opening = source.index("{", start)
end, depth = opening + 1, 1
while depth:
    depth += (source[end] == "{") - (source[end] == "}")
    end += 1
harness = r'''
#include <cassert>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <ctime>
#include <cstring>
#include <string>
#include <algorithm>
#include <limits>
constexpr int MALLOC_CAP_INTERNAL=1,MALLOC_CAP_DMA=2;
constexpr size_t kFt8LogMinDmaBlock=4096,kFt8LogMaxFileBytes=4*1024*1024;
uint32_t g_ft8_log_stage_rows=1,g_ft8_log_stage_day=20000,g_ft8_log_last_try_ms=0;
uint32_t g_ft8_log_errors=0,g_ft8_log_rows=0,g_ft8_log_dropped=0;
char buffer[1024]="row-one\n";char* g_ft8_log_stage=buffer;size_t g_ft8_log_stage_len=8;
uint32_t millis(){static uint32_t now=1;return now+=30000;}
size_t heap_caps_get_largest_free_block(int){return 65536;}
struct {template<class... T> void printf(const char*,T...){} void println(const char*){}} Serial;
const char* FILE_APPEND="a";
std::string contents;
bool exists=false,fail_open=false,fail_sync=false,fail_close=false,fail_seek=false;
bool status_error=false;
int short_call=0,write_calls=0;
size_t short_bytes=3;
struct File {
 bool valid=false,append=false;size_t pos=0;
 explicit operator bool()const{return valid;}
 size_t size()const{return contents.size();}
 bool seek(size_t offset){if(fail_seek)return false;pos=offset;return true;}
 size_t write(const uint8_t* data,size_t size){
  if(append)pos=contents.size();
  ++write_calls;if(write_calls==short_call)size=std::min(size,short_bytes);
  if(contents.size()<pos+size)contents.resize(pos+size);
  std::memcpy(contents.data()+pos,data,size);pos+=size;return size;
 }
 size_t print(const char* text){return write(reinterpret_cast<const uint8_t*>(text),std::strlen(text));}
 bool sync(){return !fail_sync;}
 bool close(){valid=false;return !fail_close;}
};
namespace orcsdr::storage {enum class EntryStatus{missing,file,directory,error};}
struct Fs {
 bool mkdir(const char*){return true;}
 bool exists(const char*){return ::exists;}
 orcsdr::storage::EntryStatus status(const char*){
  if(status_error)return orcsdr::storage::EntryStatus::error;
  return ::exists?orcsdr::storage::EntryStatus::file:orcsdr::storage::EntryStatus::missing;
 }
 File open(const char*,const char* mode,bool){
  if(fail_open)return {};
  if(std::strcmp(mode,"r+")==0&&!::exists)return {};
  if(mode[0]=='w'){contents.clear();::exists=true;}
  if(mode[0]=='a')::exists=true;
  return {true,mode[0]=='a',0};
 }
} fs;
Fs* g_sd_fs=&fs;
bool ensure_tab5_sd(){return true;}
'''
harness += source[start:end]
harness += r'''
void stage(const char* text){std::strcpy(buffer,text);g_ft8_log_stage_len=std::strlen(text);g_ft8_log_stage_rows=1;}
int main(){
 const std::string header="utc,mode,band,dial_hz,audio_hz,dt_ms,snr_db,callsign,grid,kind,message\n";
 // A partial new-file header must be repaired, not treated as a complete header.
 short_call=1;assert(!ft8_log_flush(true));short_call=0;
 assert(ft8_log_flush(true));assert(contents==header+"row-one\n");
 // Partial data: retry at the same offset rather than adding duplicate prefixes.
 stage("row-two\n");short_call=write_calls+1;
 assert(!ft8_log_flush(true));short_call=0;
 assert(ft8_log_flush(true));assert(contents==header+"row-one\nrow-two\n");
 // A complete write with failed sync must not append the rows twice.
 stage("row-three\n");fail_sync=true;assert(!ft8_log_flush(true));fail_sync=false;
 assert(ft8_log_flush(true));assert(contents==header+"row-one\nrow-two\nrow-three\n");
 // Close failure is equally ambiguous; additional staged rows remain retryable.
 stage("row-four\n");fail_close=true;assert(!ft8_log_flush(true));fail_close=false;
 stage("row-four\nrow-five\n");assert(ft8_log_flush(true));
 assert(contents==header+"row-one\nrow-two\nrow-three\nrow-four\nrow-five\n");
 const auto before=contents;
 stage("row-six\n");fail_open=true;assert(!ft8_log_flush(true));fail_open=false;
 fail_seek=true;assert(!ft8_log_flush(true)&&contents==before);fail_seek=false;
 assert(ft8_log_flush(true));assert(contents==before+"row-six\n");
 const auto committed=contents;
 stage("row-seven\n");status_error=true;
 assert(!ft8_log_flush(true)&&contents==committed);status_error=false;
 fail_sync=true;assert(!ft8_log_flush(true));fail_sync=false;
 const auto ambiguous=contents;exists=false;
 assert(!ft8_log_flush(true)&&contents==ambiguous);exists=true;
 assert(ft8_log_flush(true)&&contents==committed+"row-seven\n");
 assert(g_ft8_log_stage_rows==0&&g_ft8_log_stage_len==0);
 std::puts("PASS: FT8 retries repair partial header/data and do not duplicate after sync/close failures");
}
'''
folder = root / ".local/maps/ft8-log-retry"
folder.mkdir(parents=True, exist_ok=True)
cpp, exe = folder / "test.cpp", folder / "test"
cpp.write_text(harness, encoding="utf-8")


def linux(path):
    return "/mnt/" + str(path)[0].lower() + str(path)[2:].replace("\\", "/")


if shutil.which("g++"):
    compile_command = ["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror", str(cpp), "-o", str(exe)]
    run_command = [str(exe)]
else:
    compile_command = ["wsl", "g++", "-std=c++17", "-Wall", "-Wextra", "-Werror", linux(cpp), "-o", linux(exe)]
    run_command = ["wsl", linux(exe)]
subprocess.run(compile_command, check=True)
subprocess.run(run_command, check=True)
