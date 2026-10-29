#include "map_catalog_fetch.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
namespace c=orcsdr::maps::catalog;
namespace t=orcsdr::maps::transfer;
struct Stream:t::Stream {
  t::Headers headers{200,true,7,"","",""};
  std::string body="catalog";size_t at=0;int opens=0,closes=0;bool failed=false,read_error=false;
  bool open(std::string_view,const orcsdr::maps::download::Request&,t::Headers& out)override {
    ++opens;out=headers;return !failed;
  }
  int read(uint8_t* out,size_t size)override {
    if(read_error)return -1;
    const auto take=std::min({size,body.size()-at,size_t(3)});
    std::memcpy(out,body.data()+at,take);at+=take;return static_cast<int>(take);
  }
  void close()override{++closes;}
};
int main() {
  int failures=0;uint8_t buffer[32];
  auto check=[&](bool ok,const char* message){if(!ok){++failures;std::printf("FAIL %s\n",message);}};
  auto fetch=[&](Stream& stream,std::string& output,size_t limit=128,t::Hooks hooks={}) {
    return c::fetch(stream,"https://maps.example.test/maps-catalog-v1.json",limit,buffer,sizeof(buffer),hooks,&output);
  };
  Stream good;std::string output="unchanged";
  check(fetch(good,output)==c::FetchError::none&&output=="catalog"&&good.closes==1,"bounded catalog body fetched and closed");
  for(int status:{206,301,404,500}) {
    Stream bad;bad.headers.status=status;output="unchanged";
    check(fetch(bad,output)==c::FetchError::response&&output=="unchanged"&&bad.closes==1,"non-200 never parsed as catalog");
  }
  Stream large;large.headers.length=129;output="unchanged";
  check(fetch(large,output)==c::FetchError::limit&&large.at==0&&output=="unchanged","declared oversized response rejected before body");
  Stream chunked;chunked.headers.has_length=false;chunked.body.assign(129,'x');output="unchanged";
  check(fetch(chunked,output)==c::FetchError::limit&&output=="unchanged","chunked size cap enforced");
  Stream exact;exact.headers.has_length=false;exact.body.assign(128,'x');
  check(fetch(exact,output)==c::FetchError::none&&output.size()==128,"exact capped chunked body allowed");
  Stream truncated;truncated.body="short";output="unchanged";
  check(fetch(truncated,output)==c::FetchError::response&&output=="unchanged","truncated declared body rejected");
  Stream encoded;encoded.headers.encoding="gzip";
  check(fetch(encoded,output)==c::FetchError::response&&encoded.at==0,"unexpected encoding refused");
  Stream broken;broken.read_error=true;
  check(fetch(broken,output)==c::FetchError::network&&broken.closes==1,"read failure closes connection");
  Stream failed;failed.failed=true;
  check(fetch(failed,output)==c::FetchError::network&&failed.closes==1,"failed open still closed");
  bool cancelled=true;Stream untouched;
  t::Hooks hooks{&cancelled,[](void* value){return *static_cast<bool*>(value);}};
  check(fetch(untouched,output,128,hooks)==c::FetchError::cancelled&&untouched.opens==0,"cancel before network");
  Stream unsafe;
  check(c::fetch(unsafe,"http://maps.test/x",128,buffer,sizeof(buffer),{},&output)==c::FetchError::input&&
        unsafe.opens==0,"HTTPS required independently of transport");
  std::printf("RESULT: catalog fetch %s\n",failures?"FAIL":"PASS");return failures?1:0;
}
