#pragma once

#include <cstdint>
#include <driver/sdmmc_host.h>
#include <esp_memory_utils.h>

namespace orcsdr::storage {

inline bool sdmmc_dma_buffer_aligned(int slot, const void* buffer, size_t size) {
  if (!buffer || !size) return false;
  const auto begin = reinterpret_cast<uintptr_t>(buffer);
  if (size - 1 > UINTPTR_MAX - begin) return false;
  const auto* last = reinterpret_cast<const void*>(begin + size - 1);
  // IDF 5.5.4 checks alignment alone; TCM/RTC heaps need its sector bounce path.
  const bool internal = esp_ptr_dma_capable(buffer) && esp_ptr_dma_capable(last);
  const bool external = esp_ptr_dma_ext_capable(buffer) && esp_ptr_dma_ext_capable(last);
  return (internal || external) && sdmmc_host_check_buffer_alignment(slot, buffer, size);
}

}  // namespace orcsdr::storage
