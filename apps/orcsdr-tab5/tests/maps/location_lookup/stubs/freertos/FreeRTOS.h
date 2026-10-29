#pragma once
#include <cstdlib>
#include <cstdio>
#include <cstring>
using portMUX_TYPE=int;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(lock) ((void)(lock))
#define portEXIT_CRITICAL(lock) ((void)(lock))
#ifdef _WIN32
inline size_t strlcpy(char* out,const char* in,size_t capacity) {
  const size_t length=std::strlen(in);
  if(capacity){const size_t n=length<capacity?length:capacity-1;std::memcpy(out,in,n);out[n]=0;}
  return length;
}
#endif
