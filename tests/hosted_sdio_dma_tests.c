#include <assert.h>
#include <stddef.h>
#include <string.h>

#define EH_HOST_USE_MEMPOOL 0
#define MAX_SDIO_BUFFER_SIZE 2048
#define HOSTED_MEM_ALIGNMENT_64 64

static unsigned legacy_calls, aligned_calls;
static int fail;
static unsigned char buffer[MAX_SDIO_BUFFER_SIZE];

static void *eh_host_port_dma_alloc(size_t size) {
    (void)size;
    ++legacy_calls;
    return buffer;
}

static void *eh_host_port_dma_alloc_aligned(size_t size, size_t alignment) {
    assert(size == sizeof(buffer));
    assert(alignment == HOSTED_MEM_ALIGNMENT_64);
    ++aligned_calls;
    return fail ? NULL : buffer;
}

#include "sdio_buffer_alloc.inc"

int main(void) {
    memset(buffer, 0xa5, sizeof(buffer));
    assert(sdio_buffer_alloc(1) == buffer);
    assert(aligned_calls == 1 && legacy_calls == 0);
    for (size_t i = 0; i < sizeof(buffer); ++i) assert(buffer[i] == 0);

    memset(buffer, 0xa5, sizeof(buffer));
    assert(sdio_buffer_alloc(0) == buffer);
    for (size_t i = 0; i < sizeof(buffer); ++i) assert(buffer[i] == 0xa5);

    fail = 1;
    assert(sdio_buffer_alloc(1) == NULL);
    assert(aligned_calls == 3 && legacy_calls == 0);
}
