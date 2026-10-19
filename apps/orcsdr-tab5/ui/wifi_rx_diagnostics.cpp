// Test-build instrumentation only; leaves delivery and buffer ownership unchanged.
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_netif_net_stack.h>
#include <lwip/netif.h>
#include <atomic>
#include <cstring>

extern "C" esp_err_t __real_esp_netif_receive(esp_netif_t*, void*, size_t, void*);

extern "C" esp_err_t __wrap_esp_netif_receive(esp_netif_t* handle, void* buffer,
                                             size_t length, void* owner) {
  static std::atomic<unsigned> packets{0};
  const unsigned sequence = packets.fetch_add(1, std::memory_order_relaxed) + 1;
  const auto* interface = handle
      ? static_cast<const netif*>(esp_netif_get_netif_impl(handle)) : nullptr;
  const auto input = interface ? interface->input : nullptr;
  const unsigned flags = interface ? interface->flags : 0;
  // Keep the object on this task's captured stack: normal core dumps omit heap.
  netif snapshot{};
  if (interface) std::memcpy(&snapshot, interface, sizeof(snapshot));
  asm volatile("" : : "g"(&snapshot) : "memory");
  // Log the first traffic and periodic snapshots; always preserve a bad callback
  // observation before the original receive path executes it.
  if (sequence <= 8 || sequence % 256 == 0 || !input) {
    ESP_LOGW("wifi_rx_diag", "RX sequence=%u handle=%p netif=%p input=%p flags=%u "
             "bytes=%u dma_free=%u dma_largest=%u", sequence, handle, interface,
             reinterpret_cast<void*>(input), flags, static_cast<unsigned>(length),
             heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA),
             heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA));
  }
  const esp_err_t result = __real_esp_netif_receive(handle, buffer, length, owner);
  asm volatile("" : : "g"(&snapshot) : "memory");
  return result;
}
