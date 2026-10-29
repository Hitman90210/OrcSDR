"""Compare production LoRa preprocessing under an output-sized memory budget."""
from pathlib import Path
import shutil
import subprocess

root = Path(__file__).resolve().parents[1]
source = (root / "apps/orcsdr-tab5/ui/lora_native_decoder.cpp").read_text()


def function(name, result="bool"):
    start = source.index(result + " " + name + "(")
    opening = source.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


coefficients = source[source.index("constexpr float kLoraLowpass"):source.index("constexpr size_t kMaxBytes")]
harness = r'''
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <numeric>
#include <vector>
#include <cstdio>
constexpr uint32_t kDecodeRate=500000;
constexpr int MALLOC_CAP_SPIRAM=1,MALLOC_CAP_8BIT=2;
struct { uint8_t* resampled=nullptr; size_t resampled_capacity=0; } g_scratch;
size_t allocation_limit=SIZE_MAX, largest_request=0;
bool deny_allocation=false;
void* heap_caps_malloc(size_t n,int caps) {
 assert(caps==(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
 largest_request=std::max(largest_request,n);
 return deny_allocation || n>allocation_limit ? nullptr : std::malloc(n);
}
void heap_caps_free(void* p){std::free(p);}
void vTaskDelay(int){}
void reset(){heap_caps_free(g_scratch.resampled);g_scratch={};largest_request=0;}
'''
harness += coefficients
harness += function("filter_capture") + function("linear_resample_capture")
harness += function("compact_preprocess_capture")
harness += r'''
constexpr uint8_t kMinSf=7,kMaxSf=12;
struct Config{size_t candidate_samples=0;bool trace=false;};
struct Stats{uint32_t filter_millis=0,decode_millis=0;};
struct Packet{};
std::vector<uint8_t> expected_buffer;
size_t expected_candidate=0;
uint32_t now_millis(){static uint32_t t=0;return ++t;}
size_t decode_capture_impl(const uint8_t* p,size_t bytes,uint32_t rate,uint8_t sf,
 uint32_t bw,uint32_t hz,const Config& c,Packet*,size_t,Stats*,bool preprocessed){
 assert(preprocessed && rate==500000 && sf==11 && bw==250000 && hz==906875000);
 assert(bytes==expected_buffer.size() && c.candidate_samples==expected_candidate && !c.trace);
 assert(std::memcmp(p,expected_buffer.data(),bytes)==0);
 return 3;
}
'''
harness += function("decode_capture_in_place", "size_t")
harness += r'''
int main(){
 uint32_t seed=37;
 for(size_t samples: {size_t(2),size_t(17),size_t(8193),size_t(960000),size_t(3840000)}) {
  std::vector<uint8_t> input(samples*2);
  for(auto& x:input){seed=seed*1664525u+1013904223u;x=seed>>24;}
  const auto original=input;
  const uint8_t *filtered=nullptr,*reference=nullptr,*actual=nullptr;
  size_t filtered_n=0,reference_n=0,actual_n=0;
  allocation_limit=SIZE_MAX;
  assert(filter_capture(input.data(),samples,960000,&filtered,&filtered_n));
  assert(linear_resample_capture(filtered,filtered_n,960000,&reference,&reference_n));
  const std::vector<uint8_t> expected(reference,reference+reference_n*2);
  reset();allocation_limit=reference_n*2;
  assert(compact_preprocess_capture(input.data(),samples,960000,&actual,&actual_n));
  assert(actual_n==reference_n && largest_request<=reference_n*2);
  assert(std::memcmp(actual,expected.data(),expected.size())==0 && input==original);
  reset();deny_allocation=true;
  auto owned=input;
  assert(compact_preprocess_capture(owned.data(),samples,960000,&actual,&actual_n,owned.data()));
  assert(actual==owned.data() && actual_n==reference_n && largest_request==0);
  assert(std::memcmp(actual,expected.data(),expected.size())==0);
  owned=input;expected_buffer=expected;
  Config config;config.candidate_samples=samples/2;
  expected_candidate=uint64_t(config.candidate_samples)*500000/960000;
  Packet packet;Stats stats;
  assert(decode_capture_in_place(owned.data(),owned.size(),960000,11,250000,906875000,
                                 config,&packet,1,&stats)==3 && largest_request==0);
  assert(stats.filter_millis>0 && stats.decode_millis>0);
  owned=input;config.trace=true;
  assert(decode_capture_in_place(owned.data(),owned.size(),960000,11,250000,906875000,
                                 config,&packet,1,&stats)==0 && owned==input);
  deny_allocation=false;
  reset();deny_allocation=true;
  assert(!compact_preprocess_capture(input.data(),samples,960000,&actual,&actual_n));
  assert(!g_scratch.resampled && !g_scratch.resampled_capacity);
  deny_allocation=false;
  assert(compact_preprocess_capture(input.data(),samples,960000,&actual,&actual_n));
  assert(std::memcmp(actual,expected.data(),expected.size())==0);
  reset();
 }
 const uint8_t input[]={0,255};const uint8_t* output=nullptr;size_t n=0;
 assert(!compact_preprocess_capture(nullptr,1,960000,&output,&n));
 assert(!compact_preprocess_capture(input,0,960000,&output,&n));
 assert(!compact_preprocess_capture(input,1,480000,&output,&n));
 std::puts("PASS: byte-identical preprocessing; zero-allocation owned buffer; immutable path; failure/retry");
}
'''
folder = root / ".local/maps/lora-preprocess-memory"
folder.mkdir(parents=True, exist_ok=True)
cpp = folder / "test.cpp"
cpp.write_text(harness)
exe = folder / "test"
if shutil.which("g++"):
    compile_command = ["g++", "-std=c++17", "-O2", "-Wall", "-Wextra", "-Werror", str(cpp), "-o", str(exe)]
    run_command = [str(exe)]
else:
    linux = lambda path: "/mnt/" + str(path)[0].lower() + str(path)[2:].replace("\\", "/")
    compile_command = ["wsl", "g++", "-std=c++17", "-O2", "-Wall", "-Wextra", "-Werror", linux(cpp), "-o", linux(exe)]
    run_command = ["wsl", linux(exe)]
subprocess.run(compile_command, check=True)
subprocess.run(run_command, check=True)
