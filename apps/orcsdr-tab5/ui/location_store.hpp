#pragma once
#include <cstdint>
#include <cstddef>
namespace orcsdr::location_store {
struct Point {bool configured=false;int32_t latitude=0,longitude=0;};
struct Record {uint32_t version=1;int32_t latitude=0,longitude=0;uint32_t configured=0;};
static_assert(sizeof(Record)==16);
constexpr const char* kKey="loc_point";
inline bool valid(Point p) {
  return p.latitude>=-900000000&&p.latitude<=900000000&&p.longitude>=-1800000000&&p.longitude<=1800000000;
}
template<class Store> bool read_record(Store& store,Record* record) {
  return record&&store.getBytesLength(kKey)==sizeof(Record)&&
      store.getBytes(kKey,record,sizeof(Record))==sizeof(Record)&&record->version==1&&
      record->configured<=1&&valid({record->configured!=0,record->latitude,record->longitude});
}
template<class Store> Point load(Store& store) {
  Record record;
  if(read_record(store,&record))return {record.configured!=0,record.latitude,record.longitude};
  return {store.getBool("adsb_loc_set",false),store.getInt("adsb_lat_e7",0),store.getInt("adsb_lon_e7",0)};
}
template<class Store> bool save(Store& store,Point point) {
  if(!valid(point))return false;
  Record previous;
  if(store.isKey(kKey)&&!read_record(store,&previous))return false;
  // One blob replacement, not three separately committed keys. Legacy keys
  // remain intact for migration/rollback; no background migration writes.
  const Record next{1,point.latitude,point.longitude,point.configured?1u:0u};
  if(!store.putBytes(kKey,&next,sizeof(next)))return false;
  Record verified;
  return read_record(store,&verified)&&verified.latitude==next.latitude&&
      verified.longitude==next.longitude&&verified.configured==next.configured;
}
} // namespace orcsdr::location_store
