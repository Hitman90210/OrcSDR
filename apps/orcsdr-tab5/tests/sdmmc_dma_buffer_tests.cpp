#include <cstdint>
#include <cstdio>
#include <limits>
#include <initializer_list>
#include "esp_memory_utils.h"
#include "driver/sdmmc_host.h"

#include "sdmmc_dma_buffer.hpp"
constexpr auto accepts = orcsdr::storage::sdmmc_dma_buffer_aligned;

bool esp_ptr_dma_capable(const void* p) {
  const auto address = reinterpret_cast<uintptr_t>(p);
  return address >= 0x4ff00000 && address < 0x4ffc0000;
}
bool esp_ptr_dma_ext_capable(const void* p) {
  const auto address = reinterpret_cast<uintptr_t>(p);
  // Model this board's mapped 32 MiB PSRAM, not the whole virtual aperture.
  return address >= 0x48000000 && address < 0x4a000000;
}
bool sdmmc_host_check_buffer_alignment(int, const void* p, size_t size) {
  // ESP-IDF 5.5.4's P4 check uses alignment only (64 internal / 128 external).
  const auto address = reinterpret_cast<uintptr_t>(p);
  const size_t alignment = esp_ptr_dma_ext_capable(p) ? 128 : 64;
  return p && size && address % alignment == 0 && size % alignment == 0;
}
const void* at(uintptr_t address) { return reinterpret_cast<const void*>(address); }

#define CHECK(condition) do { if (!(condition)) { std::fprintf(stderr, "FAILED: %s\n", #condition); return 1; } } while (0)

int main() {
  // These aligned CPU heaps reproduce cache_msync's invalid-address failure.
  CHECK(sdmmc_host_check_buffer_alignment(0, at(0x30100100), 512));
  CHECK(sdmmc_host_check_buffer_alignment(0, at(0x50108100), 512));
  CHECK(!accepts(0, at(0x30100100), 512));
  CHECK(!accepts(0, at(0x50108100), 512));
  CHECK(!accepts(0, at(0x40000100), 512)); // flash is not a DMA buffer
  CHECK(!accepts(0, at(0x4a000000), 512)); // outside mapped PSRAM
  CHECK(accepts(0, at(0x4ff10000), 512));
  CHECK(accepts(0, at(0x48001000), 2048));
  // Preserve direct DMA for existing capture/export and 32 KiB stdio writes.
  for (const size_t bytes : {size_t{512}, size_t{2048}, size_t{4096},
                             size_t{32768}, size_t{65536}, size_t{131072}}) {
    CHECK(accepts(0, at(0x48001000), bytes));
    CHECK(!accepts(0, at(0x30100100), bytes));
  }
  CHECK(accepts(0, at(0x4ffbfe00), 512)); // last complete internal sector
  CHECK(accepts(0, at(0x49fffe00), 512)); // last complete PSRAM sector
  CHECK(!accepts(0, at(0x4ffbff00), 512)); // crosses memory boundary
  CHECK(!accepts(0, at(0x49ffff00), 512));
  CHECK(!accepts(0, at(0x48001040), 512)); // external cache alignment
  CHECK(!accepts(0, at(0x4ff10004), 512));
  CHECK(!accepts(0, at(0x4ff10000), 513));
  CHECK(!accepts(0, nullptr, 512));
  CHECK(!accepts(0, at(0x48001000), 0));
  CHECK(!accepts(0, at(0x48001000), std::numeric_limits<size_t>::max()));
  std::puts("SD_DMA_BUFFER_PASS: TCM/RTC rejection, valid DMA/PSRAM, ranges and alignment");
}
