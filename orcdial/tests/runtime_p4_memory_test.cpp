#include "../src/control/secure_runtime.hpp"
#include <cassert>
#include <iostream>

int main() {
  const uint8_t mac[6]={0,0,0,0,0,2};
  orc::secure::Runtime runtime;
  assert(runtime.prepare_entropy());
  task_force_failure=true;
  for (int attempt=0;attempt<100;++attempt) {
    operation=0;
    assert(!runtime.begin(2,mac,false,[](const uint8_t*,const uint8_t*){return true;}));
    // No fallback into DMA-capable memory when the CPU-only stack cannot start.
    if(last_stack_caps!=(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT|MALLOC_CAP_RTCRAM)) {
      std::cerr<<"FAIL: P4 pairing stack still uses the general internal/DMA pool\n";
      return 1;
    }
    assert(last_stack_bytes==12288);
    assert(live_queues==0 && live_stores==0);
    assert(runtime.status().failure==orc::secure::Failure::transport);
  }
  for(int stage=1;stage<=3;++stage) {
    fault_step=stage;operation=0;
    assert(!runtime.begin(2,mac,false,[](const uint8_t*,const uint8_t*){return true;}));
    assert(live_queues==0 && live_stores==0);
    assert(internal_queue_creates==0);
  }
  std::cout<<"PASS: P4 pairing stack uses internal RTC RAM; failed starts clean up without a DMA fallback\n";
}
