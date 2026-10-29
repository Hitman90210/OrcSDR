#include "map_catalog.hpp"
#include "map_catalog_session.hpp"
#include <cJSON.h>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <cstdio>
#include <string>

namespace d=orcsdr::maps::delivery;
struct Store:d::Store {
  std::string bytes;bool fail=false;
  d::Read read(std::string* out)override {
    if(bytes.empty())return d::Read::missing;
    *out=bytes;return d::Read::ok;
  }
  bool write(std::string_view value)override {if(fail)return false;bytes=value;return true;}
};

std::string load(const char* path) {
  std::ifstream file(path,std::ios::binary);
  return {std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
}
int main(int argc,char** argv) {
  if(argc!=11&&argc!=12)return 2;
  const uint64_t expected_revision=argc==12?std::stoull(argv[11]):1;
  const auto payload=load(argv[1]),signature=load(argv[2]),key=load(argv[3]);
  namespace c=orcsdr::maps::catalog;
  c::Policy policy{};
  policy.utc_now=std::stoull(argv[5]);policy.accepted_revision=std::stoull(argv[6]);
  policy.accepted_sha256=std::string_view(argv[7])=="-"?std::string_view{}:argv[7];
  policy.clock_trusted=std::string_view(argv[8])=="1";
  c::Catalog output;output.revision=999;
  output.packs.resize(1);output.packs[0].manifest_json="unchanged";
  if(std::string_view(argv[10])=="1") {
    cJSON_Hooks hooks{};hooks.malloc_fn=[](size_t)->void*{return nullptr;};
    hooks.free_fn=std::free;cJSON_InitHooks(&hooks);
  }
  const auto error=c::read_signed(payload,signature,key,policy,&output);
  if(std::string_view(c::error_name(error))!=argv[4]) {
    std::printf("expected %s, got %s\n",argv[4],c::error_name(error));return 1;
  }
  if(error!=c::Error::none) {
    if(output.revision!=999||output.packs.size()!=1||output.packs[0].manifest_json!="unchanged")return 3;
  } else if(output.revision!=expected_revision||output.packs.size()!=std::stoull(argv[9])||output.sha256.size()!=64||
            output.packs[0].product_id!="global-regional")return 4;
  Store store;
  if(policy.accepted_revision) {
    d::State prior;prior.accepted_revision=policy.accepted_revision;prior.catalog_sha256=policy.accepted_sha256;
    if(d::save(store,prior)!=d::Error::none)return 5;
  }
  const auto original=store.bytes;
  c::Catalog session;session.revision=999;
  auto accepted=d::accept_catalog(store,payload,signature,key,policy.utc_now,policy.clock_trusted,&session);
  if(accepted.storage!=d::Error::none||accepted.catalog!=error)return 6;
  if(error==c::Error::none) {
    d::State persisted;
    if(!accepted.accepted()||session.revision!=expected_revision||d::load(store,&persisted)!=d::Error::none||
        persisted.accepted_revision!=session.revision||persisted.catalog_sha256!=session.sha256)return 7;
    // Same-payload acceptance remains usable without wearing flash again.
    store.fail=true;
    if(!d::accept_catalog(store,payload,signature,key,policy.utc_now,policy.clock_trusted,&session).accepted())return 8;
    // A failed first trust commit must not expose a catalog to the caller.
    store.bytes=original;session.revision=999;
    if(policy.accepted_revision==0) {
      accepted=d::accept_catalog(store,payload,signature,key,policy.utc_now,policy.clock_trusted,&session);
      if(accepted.storage!=d::Error::storage||accepted.accepted()||session.revision!=999||store.bytes!=original)return 9;
    }
  }else if(session.revision!=999||store.bytes!=original)return 10;
  return 0;
}
