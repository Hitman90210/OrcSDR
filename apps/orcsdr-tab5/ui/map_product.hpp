#pragma once
#include <string_view>
namespace orcsdr::maps {
inline bool valid_product_id(std::string_view id) {
  if(id.empty()||id.size()>63||!((id[0]>='a'&&id[0]<='z')||(id[0]>='0'&&id[0]<='9')))return false;
  for(char c:id)if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'))return false;
  return true;
}
}
