#include <intrin.h>
#define __attribute__(attributes)
static __inline unsigned int sdk_clz32(unsigned int value) {
  unsigned long index;
  return _BitScanReverse(&index,value) ? 31u-index : 32u;
}
#define __builtin_clz(value) sdk_clz32(value)
