/* Bench experiment using the IDF 5.5.4 TLSF source (Matthew Conte,
 * BSD-3-Clause). This file is independently written; no allocator is vendored. */
#include "tlsf.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

__declspec(align(64)) static unsigned char region[65536];
/* MSVC lacks ELF weak symbols. Match the disabled poisoning hooks. */
bool tlsf_check_hook(void *start,size_t bytes,bool free_block) {
  (void)start;(void)bytes;(void)free_block;return true;
}
void block_absorb_post_hook(void *start,size_t bytes,bool free_block) {
  (void)start;(void)bytes;(void)free_block;
}
static bool largest(void *ptr,size_t bytes,int used,void *context) {
  size_t *maximum=(size_t *)context;
  (void)ptr;
  if(!used && bytes>*maximum)*maximum=bytes;
  return true;
}
static int run(size_t bytes) {
  tlsf_t heap=tlsf_create_with_pool(region,bytes,bytes);
  size_t raw=0,reported;
  void *plain,*aligned;
  assert(heap);
  tlsf_walk_pool(tlsf_get_pool(heap),largest,&raw);
  reported=tlsf_fit_size(heap,raw);
  plain=tlsf_malloc(heap,40960);
  assert(plain);
  tlsf_free(heap,plain);
  aligned=tlsf_memalign(heap,64,40960);
  printf("region=%u metadata=%u raw_largest=%u reported_largest=%u plain_40960=PASS aligned64_40960=%s\n",
      (unsigned)bytes,(unsigned)tlsf_size(heap),(unsigned)raw,(unsigned)reported,aligned?"PASS":"FAIL");
  if(aligned)tlsf_free(heap,aligned);
  return aligned!=NULL;
}
int main(void) {
  assert(sizeof(size_t)==4 && sizeof(void *)==4);
  assert(!run(42528));
  assert(run(42528+1472));
  puts("PASS: 32-bit SDK TLSF reproduces unaligned-largest versus DMA-aligned allocation boundary.");
  return 0;
}
