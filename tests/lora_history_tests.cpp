#include "lora_history.hpp"
#include <cassert>
#include <cstring>
#include <string>
using namespace orcsdr;
int main() {
  lora_history::Store store;
  lora_log::Record r{}; r.sender=123; r.packet_id=1; r.port=4; r.received_utc=1700000000;
  std::strcpy(r.short_name,"TST"); std::strcpy(r.long_name,"Name, \"quoted\"\n");
  assert(store.observe(r)); assert(!store.observe(r));
  r.packet_id=2; r.port=1; r.short_name[0]=r.long_name[0]=0;
  assert(store.observe(r)); assert(std::strcmp(store.find(123)->short_name,"TST")==0);
  r.packet_id=3; r.port=4; std::strcpy(r.long_name,"Renamed"); r.received_utc++;
  assert(store.observe(r)); assert(store.find(123)->packets==3);
  assert(std::strcmp(store.find(123)->long_name,"Renamed")==0);
  char line[lora_history::kLineMax]; auto n=lora_history::format_line(r,line,sizeof(line));
  assert(n); lora_log::Record replay{};
  assert(lora_history::parse_line(line,n,&replay)); assert(replay.sender==r.sender);
  assert(std::strcmp(replay.long_name,"Renamed")==0);
  for(size_t length=1;length<n-1;++length)
    assert(!lora_history::parse_line(line,length,&replay));
  for(size_t capacity=1;capacity<=n;++capacity)
    assert(lora_history::format_line(r,line,capacity)==0);
  n=lora_history::format_line(r,line,sizeof(line));
  lora_history::Store restored;
  assert(lora_history::parse_line(line,n,&replay));
  assert(restored.observe(replay));
  assert(std::strcmp(restored.find(123)->long_name,"Renamed")==0);
  restored.clear();assert(restored.count()==0);assert(restored.observe(replay));
  replay.from_host=true;replay.sender=999;
  assert(!restored.observe(replay));assert(!restored.find(999));
  assert(!lora_history::parse_line(line,n-4,&replay));
  std::string corrupt(line,n);corrupt.replace(corrupt.find("123"),3,"4294967296");
  assert(!lora_history::parse_line(corrupt.data(),corrupt.size(),&replay));
  r.latitude_e7=455230640;r.longitude_e7=-1226764830;r.packet_id++;
  assert(store.observe(r));r.latitude_e7=r.longitude_e7=INT32_MAX;r.packet_id++;
  store.observe(r); assert(store.find(123)->latitude_e7==455230640);
  r.encrypted=true;r.port=4;std::strcpy(r.long_name,"Decoded encrypted");r.packet_id++;
  store.observe(r); assert(std::strcmp(store.find(123)->long_name,"Decoded encrypted")==0);
  r.latitude_e7=455230641;r.longitude_e7=-1226764831;r.packet_id++;
  store.observe(r);assert(store.find(123)->latitude_e7==455230641);
  r={};r.sender=123;r.packet_id=10;r.encrypted=true; // undecoded frame has no fields
  store.observe(r);assert(std::strcmp(store.find(123)->long_name,"Decoded encrypted")==0);
  assert(store.find(123)->latitude_e7==455230641);
  for(uint32_t id=1;id<300;++id){r={};r.sender=id;store.observe(r);}
  assert(store.count()==lora_history::kNodeCapacity);
  char date[16];lora_history::day_key(0,date,sizeof(date));assert(std::strcmp(date,"undated")==0);
  lora_history::day_key(1700000000,date,sizeof(date));assert(std::strcmp(date,"20231114")==0);
  assert(lora_log::self_check());
  r={};r.sender=123;r.encrypted=true;r.from_host=true;
  std::strcpy(r.text,"text, \"quoted\"");std::strcpy(r.long_name,"Name, \"quoted\"");
  n=lora_log::format_csv(line,sizeof(line),r);assert(n);
  assert(std::strstr(line,"\"text, \"\"quoted\"\"\"")!=nullptr);
  assert(std::strstr(line,"\"Name, \"\"quoted\"\"\",1,1\n")!=nullptr);
  for (char prefix : {'=', '+', '-', '@', '\t', '\r'}) {
    r.text[0]=r.short_name[0]=r.long_name[0]=prefix;
    r.text[1]=r.short_name[1]=r.long_name[1]=0;
    assert(lora_log::format_csv(line,sizeof(line),r));
    const std::string escaped = std::string("\"'") + prefix + "\"";
    const std::string csv(line);
    size_t at=0;
    for (int field=0;field<3;++field) {
      at=csv.find(escaped,at);assert(at!=std::string::npos);at+=escaped.size();
    }
    n=lora_history::format_line(r,line,sizeof(line));assert(n);
    assert(lora_history::parse_line(line,n,&replay));
    assert(replay.text[0]==prefix && replay.short_name[0]==prefix && replay.long_name[0]==prefix);
  }
  n=lora_log::format_csv(line,sizeof(line),r);assert(n);
  for(size_t capacity=1;capacity<=n;++capacity)
    assert(lora_log::format_csv(line,capacity,r)==0);
  r.received_ms=r.received_utc=r.frequency_hz=r.sender=r.destination=r.packet_id=UINT32_MAX;
  r.port=UINT16_MAX;r.snr_tenths=r.signal_tenths=INT16_MIN;
  std::memset(r.text,'"',sizeof(r.text)-1);r.text[sizeof(r.text)-1]=0;
  std::memset(r.short_name,'"',sizeof(r.short_name)-1);r.short_name[sizeof(r.short_name)-1]=0;
  std::memset(r.long_name,'"',sizeof(r.long_name)-1);r.long_name[sizeof(r.long_name)-1]=0;
  n=lora_history::format_line(r,line,sizeof(line));assert(n);
  assert(lora_history::parse_line(line,n,&replay));
  assert(std::strcmp(r.text,replay.text)==0);
  assert(lora_log::format_csv(line,512,r)>0);
}
