#include "map_delivery_state.hpp"
#include <cstdio>
#include <string>
#include <utility>
namespace d=orcsdr::maps::delivery;
struct Store final:d::Store {
  bool present=false,read_error=false,write_error=false,corrupt_write=false;
  std::string bytes;
  d::Read read(std::string* out)override {
    if(read_error)return d::Read::error;
    if(!present)return d::Read::missing;
    *out=bytes;return d::Read::ok;
  }
  bool write(std::string_view next)override {
    if(write_error)return false;
    bytes=next;present=true;if(corrupt_write)bytes.back()^=1;return true;
  }
};
int main() {
  int failures=0;
  auto check=[&](bool ok,const char* message){if(!ok){++failures;std::printf("FAIL %s\n",message);}};
  Store store;d::State state;
  check(d::load(store,&state)==d::Error::none&&state.accepted_revision==0&&state.owned.empty(),"missing state starts empty");
  state.accepted_revision=7;state.catalog_sha256=std::string(64,'a');
  const std::string path="/orcmaps/map-"+std::string(64,'b')+"-"+std::string(64,'c')+".manifest.json";
  const std::string updated="/orcmaps/map-"+std::string(64,'d')+"-"+std::string(64,'e')+".manifest.json";
  check(d::record_install(state,"global-regional",path,true)==d::Error::input,"ownership requires prior install intent");
  check(d::plan_install(state,"global-regional",path,true)==d::Error::none&&
        d::record_install(state,"global-regional",path,true)==d::Error::none,"record explicit product ownership");
  if(state.owned.empty())return 1;
  check(d::save(store,state)==d::Error::none,"save and verify state");
  d::State restored;
  check(d::load(store,&restored)==d::Error::none&&restored.accepted_revision==7&&
        restored.catalog_sha256==state.catalog_sha256&&restored.selected_product=="global-regional"&&
        restored.owned.size()==1&&restored.owned[0].manifest==path,"complete state restored after restart");
  auto old=store.bytes;
  auto planned=state;
  check(d::plan_install(planned,"global-regional",updated,true)==d::Error::none&&
        planned.owned[0].manifest==path,"pending install does not prematurely replace ownership");
  check(d::save(store,planned)==d::Error::none&&d::load(store,&restored)==d::Error::none&&
        restored.pending.product=="global-regional"&&restored.pending.manifest==updated&&restored.pending.select,
        "restart retains explicit product and selection intent before activation");
  check(d::plan_install(restored,"other",path,false)==d::Error::input&&
        restored.pending.manifest==updated,"another install cannot overwrite pending recovery intent");
  check(d::record_install(restored,"global-regional",path,true)==d::Error::input&&
        restored.owned[0].manifest==path,"completion must match durable intent");
  check(d::record_install(restored,"global-regional",updated,true)==d::Error::none&&
        restored.pending.product.empty()&&restored.owned[0].manifest==updated,
        "completion updates ownership and clears pending intent together");
  store.bytes=old;
  restored.accepted_revision=6;
  check(d::save(store,restored)==d::Error::rollback&&store.bytes==old,"revision rollback never writes");
  restored.accepted_revision=7;restored.catalog_sha256=std::string(64,'f');
  check(d::save(store,restored)==d::Error::rollback&&store.bytes==old,"same revision equivocation refused");
  restored=state;store.read_error=true;
  check(d::save(store,restored)==d::Error::storage&&store.bytes==old,"read errors cannot reset trust");
  store.read_error=false;store.bytes.back()^=1;
  const auto damaged=store.bytes;
  check(d::load(store,&restored)==d::Error::corrupt&&restored.accepted_revision==7&&
        d::save(store,state)==d::Error::corrupt&&store.bytes==damaged,"damaged state preserved without overwriting output");
  store.bytes=old;store.write_error=true;restored=state;restored.accepted_revision=8;
  check(d::save(store,restored)==d::Error::storage&&store.bytes==old,"commit failure remains failure");
  store.write_error=false;store.corrupt_write=true;
  check(d::save(store,restored)==d::Error::storage,"readback corruption cannot report success");
  store.corrupt_write=false;store.bytes=old;
  check(d::plan_install(state,"global-regional",updated,false)==d::Error::none&&
        d::record_install(state,"global-regional",updated,false)==d::Error::none&&
        state.owned.size()==1&&state.owned[0].manifest==updated,"new artifact replaces only explicitly owned product");
  check(d::record_install(state,"other",updated,false)==d::Error::input,"one artifact cannot have two owners");
  const auto count=state.owned.size();
  check(d::record_install(state,"../manual",path,false)==d::Error::input&&state.owned.size()==count,
        "unsafe product identity rejected");
  check(d::record_install(state,"local","/orcmaps/manual.manifest.json",false)==d::Error::input,
        "manual pack paths never become managed ownership");
  auto invalid=state;invalid.selected_product="not-installed";
  check(d::save(store,invalid)==d::Error::input&&store.bytes==old,"selection must name an owned product");
  invalid=state;invalid.owned.push_back(invalid.owned[0]);
  check(d::save(store,invalid)==d::Error::input,"duplicate products rejected");
  for(int i=1;i<16;++i) {
    const std::string hash=std::string(62,'b')+"0123456789abcdef"[i]+"0";
    const std::string id="regional-"+std::to_string(i);
    const std::string target="/orcmaps/map-"+hash+"-"+std::string(64,'c')+".manifest.json";
    check(d::plan_install(state,id,target,false)==d::Error::none&&d::record_install(state,id,
        "/orcmaps/map-"+hash+"-"+std::string(64,'c')+".manifest.json",false)==d::Error::none,
        "bounded ownership accepts sixteen products");
  }
  check(d::plan_install(state,"overflow",path,false)==d::Error::limit&&state.owned.size()==16,"ownership bounded");
  store.present=false;store.bytes.clear();
  check(d::save(store,state)==d::Error::none&&store.bytes.size()<=d::kMaximumStateBytes,"maximum state roundtrip bounded");
  const auto complete=store.bytes;
  for(size_t n:{size_t(0),size_t(7),complete.size()-1}) {
    store.bytes=complete.substr(0,n);
    check(d::load(store,&restored)==d::Error::corrupt,"truncated state rejected");
  }
  std::printf("RESULT: delivery state %s\n",failures?"FAIL":"PASS");return failures?1:0;
}
