#include "map_delivery_state.hpp"
#include "map_product.hpp"
#include <mbedtls/sha256.h>
#include <algorithm>
#include <cstring>
#include <new>
namespace orcsdr::maps::delivery {
namespace {
constexpr std::string_view kMagic="OMSTATE1",kPath="/orcmaps/map-",kSuffix=".manifest.json";
bool digest(std::string_view value) {
  return value.size()==64&&std::all_of(value.begin(),value.end(),[](char c){
    return (c>='0'&&c<='9')||(c>='a'&&c<='f');});
}
bool manifest(std::string_view path) {
  return path.size()==kPath.size()+129+kSuffix.size()&&path.substr(0,kPath.size())==kPath&&
      path.substr(path.size()-kSuffix.size())==kSuffix&&path[kPath.size()+64]=='-'&&
      digest(path.substr(kPath.size(),64))&&digest(path.substr(kPath.size()+65,64));
}
bool valid(const State& state) {
  if(state.accepted_revision>9007199254740991ULL||state.owned.size()>kMaximumOwnedPacks||
      (state.accepted_revision?!digest(state.catalog_sha256):
          (!state.catalog_sha256.empty()||!state.owned.empty()||!state.selected_product.empty()||
           !state.pending.product.empty())))return false;
  const auto& pending=state.pending;
  if(pending.product.empty()) {
    if(!pending.manifest.empty()||pending.select)return false;
  }else if(!valid_product_id(pending.product)||!manifest(pending.manifest))return false;
  bool selected=state.selected_product.empty();
  for(size_t i=0;i<state.owned.size();++i) {
    const auto& pack=state.owned[i];
    if(!valid_product_id(pack.product)||!manifest(pack.manifest))return false;
    if(pack.product!=pending.product&&pack.manifest==pending.manifest)return false;
    selected|=pack.product==state.selected_product;
    for(size_t j=0;j<i;++j)
      if(state.owned[j].product==pack.product||state.owned[j].manifest==pack.manifest)return false;
  }
  return selected;
}
std::string checksum(std::string_view bytes) {
  std::string result(32,'\0');
  if(mbedtls_sha256(reinterpret_cast<const uint8_t*>(bytes.data()),bytes.size(),
      reinterpret_cast<uint8_t*>(result.data()),0))return {};
  return result;
}
void integer(std::string& bytes,uint64_t value,size_t width) {
  for(size_t i=0;i<width;++i)bytes+=static_cast<char>(value>>(i*8));
}
std::string encode(const State& state) {
  std::string bytes(kMagic);integer(bytes,state.accepted_revision,8);
  bytes+=state.accepted_revision?state.catalog_sha256:std::string(64,'0');
  integer(bytes,state.owned.size(),1);integer(bytes,state.selected_product.size(),1);
  bytes+=state.selected_product;
  const auto& pending=state.pending;integer(bytes,pending.product.empty()?0:1,1);
  if(!pending.product.empty()) {
    integer(bytes,pending.product.size(),1);bytes+=pending.product;
    integer(bytes,pending.manifest.size(),2);bytes+=pending.manifest;
    integer(bytes,pending.select?1:0,1);
  }
  for(const auto& pack:state.owned) {
    integer(bytes,pack.product.size(),1);bytes+=pack.product;
    integer(bytes,pack.manifest.size(),2);bytes+=pack.manifest;
  }
  const auto sum=checksum(bytes);if(sum.size()!=32)return {};
  bytes+=sum;return bytes;
}
struct Cursor {
  std::string_view bytes;size_t at=0;
  bool number(size_t width,uint64_t& value) {
    if(width>bytes.size()-at)return false;
    value=0;for(size_t i=0;i<width;++i)value|=uint64_t{static_cast<uint8_t>(bytes[at++])}<<(i*8);
    return true;
  }
  bool text(size_t size,std::string& value) {
    if(size>bytes.size()-at)return false;
    value=bytes.substr(at,size);at+=size;return true;
  }
};
bool decode(std::string_view bytes,State& state) {
  if(bytes.size()<115||bytes.size()>kMaximumStateBytes||bytes.substr(0,8)!=kMagic)return false;
  const auto body=bytes.substr(0,bytes.size()-32);
  if(checksum(body)!=bytes.substr(body.size()))return false;
  Cursor cursor{body,8};uint64_t count=0,size=0;
  if(!cursor.number(8,state.accepted_revision)||!cursor.text(64,state.catalog_sha256)||
      !cursor.number(1,count)||count>kMaximumOwnedPacks||!cursor.number(1,size)||size>63||
      !cursor.text(size,state.selected_product))return false;
  if(!state.accepted_revision) {
    if(state.catalog_sha256!=std::string(64,'0'))return false;
    state.catalog_sha256.clear();
  }
  if(!cursor.number(1,size)||size>1)return false;
  if(size) {
    if(!cursor.number(1,size)||size>63||!cursor.text(size,state.pending.product)||
        !cursor.number(2,size)||size>256||!cursor.text(size,state.pending.manifest)||
        !cursor.number(1,size)||size>1)return false;
    state.pending.select=size!=0;
  }
  state.owned.reserve(count);
  for(size_t i=0;i<count;++i) {
    OwnedPack pack;
    if(!cursor.number(1,size)||size>63||!cursor.text(size,pack.product)||
        !cursor.number(2,size)||size>256||!cursor.text(size,pack.manifest))return false;
    state.owned.push_back(std::move(pack));
  }
  return cursor.at==body.size()&&valid(state);
}
}
Error load(Store& store,State* output) noexcept {
  if(!output)return Error::input;
  try {
    std::string bytes;const auto read=store.read(&bytes);
    if(read==Read::error)return Error::storage;
    State next;
    if(read==Read::ok&&!decode(bytes,next))return Error::corrupt;
    *output=std::move(next);return Error::none;
  }catch(const std::bad_alloc&){return Error::allocation;}
}
Error save(Store& store,const State& state) noexcept {
  try {
    if(!valid(state))return Error::input;
    State previous;const auto error=load(store,&previous);
    if(error!=Error::none)return error;
    if(state.accepted_revision<previous.accepted_revision||
        (state.accepted_revision==previous.accepted_revision&&state.catalog_sha256!=previous.catalog_sha256))
      return Error::rollback;
    const auto bytes=encode(state);
    if(bytes.empty()||bytes.size()>kMaximumStateBytes)return Error::limit;
    if(!store.write(bytes))return Error::storage;
    std::string actual;
    return store.read(&actual)==Read::ok&&actual==bytes?Error::none:Error::storage;
  }catch(const std::bad_alloc&){return Error::allocation;}
}
Error record_install(State& state,std::string_view id,std::string_view path,bool select) noexcept {
  try {
    if(!valid(state)||!state.accepted_revision||!valid_product_id(id)||!manifest(path))return Error::input;
    if(state.pending.product.empty()||state.pending.product!=id||state.pending.manifest!=path||
        state.pending.select!=select)return Error::input;
    for(const auto& pack:state.owned)if(pack.product!=id&&pack.manifest==path)return Error::input;
    State next=state;
    const auto found=std::find_if(next.owned.begin(),next.owned.end(),[&](const OwnedPack& pack){return pack.product==id;});
    if(found==next.owned.end()) {
      if(next.owned.size()>=kMaximumOwnedPacks)return Error::limit;
      next.owned.push_back({std::string(id),std::string(path)});
    }else found->manifest=path;
    if(select)next.selected_product=id;
    next.pending={};
    state=std::move(next);return Error::none;
  }catch(const std::bad_alloc&){return Error::allocation;}
}
Error plan_install(State& state,std::string_view id,std::string_view path,bool select) noexcept {
  try {
    if(!valid(state)||!state.accepted_revision||!valid_product_id(id)||!manifest(path))return Error::input;
    if(!state.pending.product.empty()&&(state.pending.product!=id||state.pending.manifest!=path||
        state.pending.select!=select))return Error::input;
    for(const auto& pack:state.owned)if(pack.product!=id&&pack.manifest==path)return Error::input;
    const auto found=std::find_if(state.owned.begin(),state.owned.end(),[&](const OwnedPack& pack){return pack.product==id;});
    if(found==state.owned.end()&&state.owned.size()>=kMaximumOwnedPacks)return Error::limit;
    State next=state;next.pending={std::string(id),std::string(path),select};
    state=std::move(next);return Error::none;
  }catch(const std::bad_alloc&){return Error::allocation;}
}
}
