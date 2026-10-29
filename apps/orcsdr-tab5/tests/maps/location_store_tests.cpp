#include "location_store.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>
namespace {
struct Store {
  std::map<std::string,std::vector<unsigned char>> data;
  int budget=99;
  bool acknowledge_without_write=false;
  bool isKey(const char* key) const {return data.count(key)!=0;}
  size_t getBytesLength(const char* key) const {auto p=data.find(key);return p==data.end()?0:p->second.size();}
  size_t getBytes(const char* key,void* out,size_t size) const {
    auto p=data.find(key);if(p==data.end()||p->second.size()!=size)return 0;
    std::memcpy(out,p->second.data(),size);return size;
  }
  bool putBytes(const char* key,const void* value,size_t size) {
    if(acknowledge_without_write)return true;
    if(budget<=0)return false;--budget;
    auto bytes=static_cast<const unsigned char*>(value);data[key]={bytes,bytes+size};return true;
  }
  bool getBool(const char* key,bool fallback) const {bool value;return getBytes(key,&value,sizeof(value))?value:fallback;}
  int32_t getInt(const char* key,int32_t fallback) const {int32_t value;return getBytes(key,&value,sizeof(value))?value:fallback;}
  bool putInt(const char* key,int32_t value) {return putBytes(key,&value,sizeof(value));}
  bool putBool(const char* key,bool value) {return putBytes(key,&value,sizeof(value));}
};
bool same(orcsdr::location_store::Point a,orcsdr::location_store::Point b) {
  return a.configured==b.configured&&a.latitude==b.latitude&&a.longitude==b.longitude;
}
}
int main() {
  using namespace orcsdr::location_store;
  Store legacy;const Point original{true,440000000,-1230000000},next{true,450000000,-1220000000};
  legacy.putBool("adsb_loc_set",original.configured);legacy.putInt("adsb_lat_e7",original.latitude);legacy.putInt("adsb_lon_e7",original.longitude);
  assert(same(load(legacy),original));
  for(int budget=0;budget<5;++budget) {
    Store device=legacy;device.budget=budget;
    const bool saved=save(device,next);
    assert(same(load(device),saved?next:original)); // never a mixed coordinate pair
  }
  Store device=legacy;
  assert(save(device,next));
  assert(device.getInt("adsb_lat_e7",0)==original.latitude); // legacy rollback record retained
  device.acknowledge_without_write=true;
  assert(!save(device,original));assert(same(load(device),next));
  device.acknowledge_without_write=false;
  device.budget=0;assert(!save(device,original));assert(same(load(device),next));
  device.budget=99;assert(save(device,{false,next.latitude,next.longitude}));
  assert(!load(device).configured);
  assert(!save(device,{true,900000001,0}));
  Record future{2,123,456,1};device.putBytes(kKey,&future,sizeof(future));
  const auto data=device.data;assert(!save(device,next));assert(device.data==data);
  assert(same(load(device),original));
  Store blank;assert(!load(blank).configured);assert(blank.data.empty());
  std::puts("LOCATION_STORE_TEST_PASS");
}
