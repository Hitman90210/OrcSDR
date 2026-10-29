#pragma once
#include "map_transfer.hpp"
#include <array>

namespace orcsdr::maps::transfer {
// Redirects may use temporary query strings, but never plaintext transport,
// embedded credentials or header-control characters. Do not log these URLs.
inline bool https_endpoint(std::string_view url) {
  if(url.size()>2048||url.substr(0,8)!="https://")return false;
  for(unsigned char c:url)if(c<=32||c==127||c=='\\'||c=='"')return false;
  const auto end=url.find_first_of("/?#",8);
  auto authority=url.substr(8,end==std::string_view::npos?url.size()-8:end-8);
  return !authority.empty()&&authority.find('@')==std::string_view::npos;
}
struct HeaderCapture {
  Headers value;
  std::string location;
  std::array<bool,5> seen{};
  bool valid=true;
  bool add(std::string_view key,std::string_view text) {
    const char* names[]={"content-length","content-range","etag","content-encoding","location"};
    const size_t limits[]={32,128,128,32,2048};
    for(size_t i=0;i<5;++i)if(download::detail::equal_ascii(key,names[i])) {
      if(seen[i]||text.size()>limits[i])return valid=false;
      for(unsigned char c:text)if(c<32||c==127)return valid=false;
      seen[i]=true;
      if(i==1)value.range=text;
      if(i==2)value.etag=text;
      if(i==3)value.encoding=text;
      if(i==4)location=text;
    }
    return valid;
  }
};
} // namespace orcsdr::maps::transfer
