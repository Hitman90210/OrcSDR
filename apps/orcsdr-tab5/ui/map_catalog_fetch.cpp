#include "map_catalog_fetch.hpp"
#include "map_http_headers.hpp"
#include <algorithm>
#include <new>
namespace orcsdr::maps::catalog {
FetchError fetch(transfer::Stream& stream,std::string_view endpoint,size_t maximum,uint8_t* buffer,size_t capacity,
                 const transfer::Hooks& hooks,std::string* output) noexcept {
  if(!output||!buffer||!capacity||!maximum||maximum>128*1024||!transfer::https_endpoint(endpoint))return FetchError::input;
  try {
    if(hooks.cancelled&&hooks.cancelled(hooks.context))return FetchError::cancelled;
    struct Close {transfer::Stream& stream;~Close(){stream.close();}} close{stream};
    transfer::Headers header;
    if(!stream.open(endpoint,{},header))return FetchError::network;
    if(header.status!=200||(!header.encoding.empty()&&header.encoding!="identity")||!header.range.empty())return FetchError::response;
    if(header.has_length&&header.length>maximum)return FetchError::limit;
    std::string next;next.reserve(header.has_length?static_cast<size_t>(header.length):maximum);
    while(true) {
      if(hooks.cancelled&&hooks.cancelled(hooks.context))return FetchError::cancelled;
      const size_t take=std::min(capacity,maximum+1-next.size());
      const int got=stream.read(buffer,take);
      if(got<0)return FetchError::network;
      if(static_cast<size_t>(got)>take)return FetchError::response;
      if(!got)break;
      if(static_cast<size_t>(got)>maximum-next.size())return FetchError::limit;
      next.append(reinterpret_cast<const char*>(buffer),static_cast<size_t>(got));
    }
    if(next.empty()||(header.has_length&&next.size()!=header.length))return FetchError::response;
    *output=std::move(next);return FetchError::none;
  }catch(const std::bad_alloc&){return FetchError::allocation;}
}
}
