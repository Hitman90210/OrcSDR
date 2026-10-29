#include "map_inventory.hpp"
#include "orcmap/pack_json.hpp"
#include <cassert>
#include <fstream>
#include <iterator>
#include <map>
#include <new>

namespace {
struct Files final : orcmap::PackFileSystem {
  std::map<std::string,std::string> manifests;
  std::map<std::string,uint64_t> archives;
  bool listed=true, fail=false;
  bool ListManifestNames(const std::string&,std::vector<std::string>* out) override {
    out->clear();for(const auto& item:manifests)out->push_back(item.first);return listed;
  }
  bool ReadTextFile(const std::string& path,size_t limit,std::string* out) override {
    if(fail)throw std::bad_alloc();
    auto it=manifests.find(path.substr(path.find_last_of('/')+1));
    if(it==manifests.end() || it->second.size()>limit)return false;
    *out=it->second;return true;
  }
  bool FileSize(const std::string& path,uint64_t* size) override {
    auto it=archives.find(path);if(it==archives.end())return false;
    *size=it->second;return true;
  }
};
void replace(std::string& text,const std::string& old,const std::string& value) {
  auto at=text.find(old);assert(at!=std::string::npos);text.replace(at,old.size(),value);
}
void number(std::string& text,const char* key,double value) {
  const auto at=text.find(std::string("\"")+key+"\":");assert(at!=std::string::npos);
  const auto start=text.find(':',at)+1,end=text.find_first_of(",}\n",start);
  text.replace(start,end-start," "+std::to_string(value));
}
void overzoom_tests(const orcmap::PackManifest& base,const std::string& original) {
  Files files;
  const auto add=[&](orcmap::PackManifest pack,const char* name) {
    pack.region_id=name;pack.pack_id=orcmap::MakePackId(pack);
    std::string json=original;
    replace(json,base.pack_id,pack.pack_id);replace(json,base.region_id,pack.region_id);
    number(json,"min_lon",pack.bounds.min_lon_deg);number(json,"max_lon",pack.bounds.max_lon_deg);
    number(json,"min_lat",pack.bounds.min_lat_deg);number(json,"max_lat",pack.bounds.max_lat_deg);
    number(json,"min_zoom",pack.min_zoom);number(json,"max_zoom",pack.max_zoom);number(json,"priority",pack.priority);
    files.manifests[std::string(name)+".manifest.json"]=json;
    files.archives[std::string("/maps/")+name+".pmtiles"]=pack.size_bytes;
    return pack.pack_id;
  };
  auto global=base;global.bounds={-180,-85,180,85};global.min_zoom=0;global.max_zoom=8;global.priority=0;
  auto local=base;local.bounds={-1,-1,1,1};local.min_zoom=5;local.max_zoom=15;
  const auto global_id=add(global,"world-test"),local_id=add(local,"local-test");
  orcsdr::maps::Inventory inventory;
  assert(inventory.scan(files,"/maps",true));
  const orcmap::GeoBounds inside{-.5,-.5,.5,.5},elsewhere{10,40,11,41},dateline{170,-20,-170,-10};
  const auto selected=[&](const orcmap::GeoBounds& bounds,uint8_t zoom,const char* preferred) {
    const auto* pack=inventory.resolve(bounds,zoom,preferred);assert(pack);return pack->pack_id;
  };
  assert(selected(inside,16,nullptr)==local_id);
  assert(selected(inside,16,local_id.c_str())==local_id);
  assert(selected(inside,16,global_id.c_str())==global_id); // explicit valid preference wins
  assert(selected(elsewhere,12,local_id.c_str())==global_id); // preference outside coverage
  assert(selected(elsewhere,12,"removed-pack")==global_id);
  assert(selected(inside,3,local_id.c_str())==global_id); // do not fetch above camera zoom
  assert(selected(dateline,12,nullptr)==global_id);
  assert(!inventory.resolve({10,86,11,87},12));
  assert(!inventory.resolve(inside,21));
  std::printf("PACK_OVERZOOM_PASS global/local source limits, bounds, preference fallback and dateline\n");
}
}
void inventory_tests(const char* path) {
  std::ifstream input(path);assert(input);
  std::string original((std::istreambuf_iterator<char>(input)),{});
  orcmap::PackManifest base;
  assert(orcmap::ParsePackManifestJson(original,&base)==orcmap::PackJsonError::kNone);
  overzoom_tests(base,original);
  using orcsdr::maps::Inventory;
  Inventory inventory;
  Files files;
  assert(!inventory.scan(files,"/maps",false) && !inventory.sd_mounted());
  auto proto=base;proto.schema_version="protomaps-v4";proto.pack_id=orcmap::MakePackId(proto);
  std::string proto_json=original;
  replace(proto_json,base.pack_id,proto.pack_id);
  replace(proto_json,"\"schema_version\": \""+base.schema_version+"\"","\"schema_version\": \"protomaps-v4\"");
  files.manifests["proto.manifest.json"]=proto_json;
  files.archives["/maps/proto.pmtiles"]=proto.size_bytes;
  assert(inventory.scan(files,"/maps",true));
  assert(inventory.find(proto.pack_id.c_str())!=nullptr);
  assert(inventory.resolve(proto.bounds,proto.min_zoom,proto.pack_id.c_str())!=nullptr);
  // Camera zoom can exceed stored tile detail without losing installed coverage.
  assert(inventory.resolve(proto.bounds,proto.max_zoom+1,proto.pack_id.c_str())!=nullptr);
  auto unknown=proto;unknown.schema_version="protomaps-v999";unknown.pack_id=orcmap::MakePackId(unknown);
  replace(proto_json,proto.pack_id,unknown.pack_id);
  replace(proto_json,"\"schema_version\": \"protomaps-v4\"","\"schema_version\": \"protomaps-v999\"");
  files.manifests["proto.manifest.json"]=proto_json;
  assert(!inventory.scan(files,"/maps",true));
  assert(inventory.report().rejected.size()==1);
  assert(inventory.report().rejected[0].validation_error==orcmap::PackValidationError::kCompatibility);
  for (size_t count:{0u,1u,5u,16u}) {
    files={};
    for(size_t i=0;i<count;++i) {
      auto pack=base;
      pack.region_id="test-region-"+std::to_string(i);
      std::string json=original;
      replace(json,base.pack_id,orcmap::MakePackId(pack));
      replace(json,"\"region_id\": \""+base.region_id+"\"","\"region_id\": \""+pack.region_id+"\"");
      const auto name="test"+std::to_string(i);
      files.manifests[name+".manifest.json"]=json;
      files.archives["/maps/"+name+".pmtiles"]=base.size_bytes;
    }
    assert(inventory.scan(files,"/maps",true));
    assert(inventory.catalog().Packs().size()==count);
    if(count) {
      auto& pack=inventory.catalog().Packs()[0];
      assert(inventory.find(pack.pack_id.c_str())==&pack);
      assert(inventory.resolve(pack.bounds,pack.min_zoom,pack.pack_id.c_str())==&pack);
    }
  }
  files.manifests["overflow.manifest.json"]=original;
  assert(!inventory.scan(files,"/maps",true) && inventory.limit_exceeded());
  assert(inventory.catalog().Packs().empty());
  files={};files.manifests["broken.manifest.json"]="{";
  assert(!inventory.scan(files,"/maps",true));
  assert(inventory.report().rejected.size()==1);
  files={};files.manifests["missing.manifest.json"]=original;
  assert(!inventory.scan(files,"/maps",true));
  assert(inventory.report().rejected[0].rejection==orcmap::PackRejection::kArchiveMissing);
  files.archives["/maps/missing.pmtiles"]=1;
  assert(!inventory.scan(files,"/maps",true));
  assert(inventory.report().rejected[0].rejection==orcmap::PackRejection::kArchiveSizeMismatch);
  files.listed=false;assert(!inventory.scan(files,"/maps",true));
  assert(!inventory.report().directory_listed);
  files.listed=true;files.archives["/maps/missing.pmtiles"]=base.size_bytes;files.fail=true;
  assert(!inventory.scan(files,"/maps",true)&&inventory.out_of_memory());
  assert(inventory.catalog().Packs().empty());
  files.fail=false;assert(inventory.scan(files,"/maps",true));
  assert(!inventory.out_of_memory()&&inventory.catalog().Packs().size()==1);
}
