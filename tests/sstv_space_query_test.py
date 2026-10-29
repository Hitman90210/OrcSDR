"""Check the actual ESP default SSTV free-space callback's failure mapping."""
from pathlib import Path
import subprocess
import shutil
root=Path(__file__).resolve().parents[1]
s=(root/'apps/orcsdr-tab5/ui/sstv_archive.cpp').read_text(encoding='utf-8')
a=s.index('uint64_t d_free_bytes() {');b=s.index('\nbool d_list_dir(',a)
h=r'''
#include <cstdint>
#include <cassert>
#include <cstdio>
#define ESP_PLATFORM 1
bool query_ok=false;uint64_t available=0;
namespace orcsdr::storage {
uint64_t free_bytes(){return query_ok?available:0;}
bool query_free_bytes(uint64_t* free){*free=available;return query_ok;}
}
'''+s[a:b]+r'''
int main(){
 assert(d_free_bytes()==UINT64_MAX); // unknown skips advisory preflight, not a false full card
 query_ok=true;assert(d_free_bytes()==0); // known empty space still fails low-space check
 available=12345;assert(d_free_bytes()==12345);
 std::puts("PASS: SSTV unknown capacity uses advisory sentinel; known zero and known free remain distinct");
}
'''
folder=root/'.local/maps/sstv-space-query';folder.mkdir(parents=True,exist_ok=True)
cpp,exe=folder/'test.cpp',folder/'test';cpp.write_text(h,encoding='utf-8')
def linux(p):return '/mnt/'+str(p)[0].lower()+str(p)[2:].replace('\\','/')
if shutil.which('g++'):
 command=['g++','-std=c++17','-Wall','-Wextra','-Werror',str(cpp),'-o',str(exe)];run=[str(exe)]
else:
 command=['wsl','g++','-std=c++17','-Wall','-Wextra','-Werror',linux(cpp),'-o',linux(exe)];run=['wsl',linux(exe)]
subprocess.run(command,check=True);subprocess.run(run,check=True)
