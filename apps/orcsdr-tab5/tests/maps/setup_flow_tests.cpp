#include "setup_flow.hpp"
#include <cassert>
#include <cstdio>
#include <initializer_list>
using namespace orcsdr::setup;
int main() {
  auto blank=Flow::restore(false,0,false);assert(blank.automatic&&!blank.done);
  auto old=Flow::restore(false,0,true);assert(!old.automatic&&old.writable);
  blank.next();assert(blank.step==Step::network);
  blank.next();assert(blank.step==Step::location);
  auto resumed=Flow::restore(true,blank.record(),true);
  assert(resumed.automatic&&resumed.step==Step::location);
  resumed.back();assert(resumed.step==Step::network);
  resumed.back();resumed.back();assert(resumed.step==Step::welcome);
  resumed.quick_start();assert(resumed.step==Step::complete&&!resumed.done);
  resumed.next();assert(resumed.step==Step::complete);
  resumed.done=true;auto saved=Flow::restore(true,resumed.record(),false);
  assert(saved.done&&!saved.automatic);
  for(uint32_t bad:{0u,0xffffffffu,0x4d020000u,0x4d010005u,0x4d010200u,0x4d010101u}) {
    auto unknown=Flow::restore(true,bad,false);
    assert(!unknown.automatic&&!unknown.writable);
  }
  int32_t coordinate=42;
  for(const char* bad:{"","-","1.2"," 12","+12","12x","900000001","-900000001","99999999999999999999"}) {
    assert(!parse_e7(bad,true,&coordinate));assert(coordinate==42);
  }
  assert(parse_e7("-900000000",true,&coordinate)&&coordinate==-900000000);
  assert(parse_e7("1800000000",false,&coordinate)&&coordinate==1800000000);
  assert(!parse_e7("1800000001",false,&coordinate));
  assert(parse_e7("-0",false,&coordinate)&&coordinate==0);
  std::puts("setup progress: blank, legacy, resume, back, quick start, future schema PASS");
}
