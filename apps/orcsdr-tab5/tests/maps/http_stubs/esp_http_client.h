#pragma once
#include <cstdint>
using esp_err_t=int;
inline constexpr esp_err_t ESP_OK=0,ESP_FAIL=-1,ESP_ERR_NO_MEM=0x101;
enum esp_http_client_event_id_t {HTTP_EVENT_ON_HEADER};
enum esp_http_client_transport_t {HTTP_TRANSPORT_UNKNOWN,HTTP_TRANSPORT_OVER_TCP,HTTP_TRANSPORT_OVER_SSL};
struct esp_http_client;
using esp_http_client_handle_t=esp_http_client*;
struct esp_http_client_event_t {
  esp_http_client_event_id_t event_id;
  void* user_data;
  char* header_key;
  char* header_value;
};
// Only the SDK declarations consumed by map HTTP/location adapters are represented.
// This is an adapter test, not a replacement TLS/HTTP implementation.
struct esp_http_client_config_t {
  const char* url=nullptr;
  const char* user_agent=nullptr;
  int timeout_ms=0,buffer_size=0,buffer_size_tx=0;
  esp_http_client_transport_t transport_type{};
  esp_err_t (*crt_bundle_attach)(void*)=nullptr;
  bool disable_auto_redirect=false;
  esp_err_t (*event_handler)(esp_http_client_event_t*)=nullptr;
  void* user_data=nullptr;
};
extern "C" {
esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t*);
esp_err_t esp_http_client_set_header(esp_http_client_handle_t,const char*,const char*);
esp_err_t esp_http_client_open(esp_http_client_handle_t,int);
int64_t esp_http_client_fetch_headers(esp_http_client_handle_t);
int esp_http_client_get_status_code(esp_http_client_handle_t);
bool esp_http_client_is_chunked_response(esp_http_client_handle_t);
bool esp_http_client_is_complete_data_received(esp_http_client_handle_t);
int esp_http_client_read(esp_http_client_handle_t,char*,int);
esp_err_t esp_http_client_close(esp_http_client_handle_t);
esp_err_t esp_http_client_cleanup(esp_http_client_handle_t);
}
