// Opt-in bench logging only. Preserve the real startup allocator and its result.
#include <esp_heap_caps.h>
#include <esp_rom_sys.h>
#include <esp_private/esp_psram_extram.h>

extern "C" esp_err_t __real_esp_psram_extram_reserve_dma_pool(size_t bytes);

extern "C" esp_err_t __wrap_esp_psram_extram_reserve_dma_pool(size_t bytes) {
  constexpr uint32_t dma = MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA;
  // ROM output does not allocate stdio state. Plain literals stay in mapped
  // read-only memory; DRAM_STR logging would itself reduce the measured budget.
  esp_rom_printf("ORC_MAP_STARTUP phase=before reserve_bytes=%u dma_free=%u dma_largest=%u internal_free=%u psram_free=%u\n",
      static_cast<unsigned>(bytes),
      static_cast<unsigned>(heap_caps_get_free_size(dma)),
      static_cast<unsigned>(heap_caps_get_largest_free_block(dma)),
      static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
      static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)));
  const esp_err_t result = __real_esp_psram_extram_reserve_dma_pool(bytes);
  esp_rom_printf("ORC_MAP_STARTUP phase=after result=%d dma_free=%u dma_largest=%u\n",
      static_cast<int>(result),
      static_cast<unsigned>(heap_caps_get_free_size(dma)),
      static_cast<unsigned>(heap_caps_get_largest_free_block(dma)));
  return result;
}
