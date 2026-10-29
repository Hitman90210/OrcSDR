#include "map_feature_cache.hpp"
#include <cassert>
#include <string>

bool name(const orcmap::Feature& f, void* context) {
  *static_cast<std::string*>(context) = std::get<std::string>(f.property_values.at(0));
  return true;
}
int main() {
  using namespace orcsdr::maps;
  uint8_t arena[512]{}; FeatureCache cache; cache.Reset(arena, sizeof(arena));
  const orcmap::TileId a{4,1,1},b{4,2,1},c{4,3,1},d{4,4,1};
  orcmap::Feature feature;feature.property_keys={"name"};feature.property_values={std::string("first")};
  for(auto tile:{a,b,c}) {
    assert(cache.Begin(tile,true));cache.Capture(feature);assert(cache.Commit(true));
    std::get<std::string>(feature.property_values[0])+="!";
  }
  assert(cache.Count()==3&&cache.Used()<=cache.Capacity());
  std::vector<orcmap::TilePlacement> visible(2);visible[0].tile=b;visible[1].tile=c;
  const size_t before=cache.Used();cache.Prepare(visible,true);
  assert(!cache.Find(a)&&cache.Find(b)&&cache.Find(c)&&cache.Count()==2&&cache.Used()<before);
  orcmap::Feature scratch;std::string value;
  assert(cache.Find(b)->Replay(scratch,name,&value)&&value=="first!");
  assert(cache.Find(c)->Replay(scratch,name,&value)&&value=="first!!");
  const size_t retained=cache.Used();
  assert(cache.Begin(d,true));feature.property_values={std::string(1024,'x')};
  cache.Capture(feature);assert(!cache.Commit(true)&&!cache.Find(d)&&cache.Used()==retained);
  assert(cache.Find(c)->Replay(scratch,name,&value)&&value=="first!!");
  feature.property_values={std::string("cancelled")};assert(cache.Begin(d,true));cache.Capture(feature);
  assert(!cache.Commit(false)&&!cache.Find(d));
  cache.Prepare(visible,false);assert(cache.Count()==0&&cache.Used()==0);
  cache.Reset();assert(!cache.Begin(a,false));cache.Prepare(visible,false);assert(!cache.Find(a));
}
