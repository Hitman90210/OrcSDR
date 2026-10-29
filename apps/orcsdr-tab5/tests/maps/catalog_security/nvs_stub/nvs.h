#pragma once
#include <cstdint>
#include <cstddef>
using nvs_handle_t=uint32_t;
using esp_err_t=int;
enum nvs_open_mode_t {NVS_READONLY,NVS_READWRITE};
constexpr esp_err_t ESP_OK=0,ESP_ERR_NVS_NOT_FOUND=1,ESP_FAIL=2;
esp_err_t nvs_open(const char*,nvs_open_mode_t,nvs_handle_t*);
void nvs_close(nvs_handle_t);
esp_err_t nvs_get_blob(nvs_handle_t,const char*,void*,size_t*);
esp_err_t nvs_set_blob(nvs_handle_t,const char*,const void*,size_t);
esp_err_t nvs_commit(nvs_handle_t);
