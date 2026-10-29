"""Fault-inject real FAT capacity wrappers without an SD card or large files."""
from pathlib import Path
import shutil
import subprocess

root = Path(__file__).resolve().parents[1]
source = (root / "apps/orcsdr-tab5/ui/orcsdr_storage.cpp").read_text(encoding="utf-8")


def function(name, result):
    start = source.index(result + " " + name + "(")
    brace = source.index("{", start)
    end, depth = brace + 1, 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


harness = r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdio>
constexpr int ESP_OK=0;
bool g_mounted=true;
int query_result=ESP_OK,calls=0;
uint64_t actual_total=32000000000ULL,actual_free=543000000;
int esp_vfs_fat_info(const char* path,uint64_t* total,uint64_t* free){
 assert(std::strcmp(path,"/sd")==0);++calls;
 *total=actual_total;*free=actual_free;return query_result;
}
'''
harness += function("sd_space", "bool")
harness += function("query_free_bytes", "bool")
for name in ("total_bytes", "used_bytes", "free_bytes"):
    harness += function(name, "uint64_t")
harness += r'''
int main(){
 uint64_t queried=0;assert(query_free_bytes(&queried)&&queried==actual_free);
 assert(!query_free_bytes(nullptr));
 assert(total_bytes()==actual_total);
 assert(used_bytes()==actual_total-actual_free);
 assert(free_bytes()==actual_free && free_bytes()<543380195);
 actual_free=0;assert(free_bytes()==0 && used_bytes()==actual_total);
 actual_free=actual_total;assert(used_bytes()==0 && free_bytes()==actual_total);
 actual_free=actual_total+1;assert(total_bytes()==0 && used_bytes()==0 && free_bytes()==0);
 query_result=-1;actual_free=actual_total/2;
 assert(!query_free_bytes(&queried)&&queried==0);
 assert(total_bytes()==0 && used_bytes()==0 && free_bytes()==0);
 const int previous=calls;g_mounted=false;
 assert(total_bytes()==0 && used_bytes()==0 && free_bytes()==0 && calls==previous);
 std::puts("PASS: real FAT capacity, nearly full/full cards, query failure, invalid values and unmounted state");
}
'''
folder = root / ".local/maps/sd-space-query"
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
