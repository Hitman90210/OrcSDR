#include <esp_err.h>
#include <esp_event.h>
#include <esp_hosted.h>
#include <esp_hosted_ota.h>
#include <esp_log.h>
#include <esp_system.h>
#include <driver/i2c_master.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <nvs_flash.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

extern const uint8_t esp_hosted_tab5_c6_bin_start[]
    asm("_binary_esp_hosted_tab5_c6_bin_start");
extern const uint8_t esp_hosted_tab5_c6_bin_end[]
    asm("_binary_esp_hosted_tab5_c6_bin_end");

static const char* TAG = "OrcSDR-bridge";

static esp_err_t prepare_tab5_c6_power(void) {
  i2c_master_bus_handle_t bus = NULL;
  i2c_master_dev_handle_t expander = NULL;
  const i2c_master_bus_config_t bus_config = {
      .i2c_port = I2C_NUM_1,
      .sda_io_num = GPIO_NUM_31,
      .scl_io_num = GPIO_NUM_32,
      .clk_source = I2C_CLK_SRC_DEFAULT,
      .glitch_ignore_cnt = 7,
      .flags.enable_internal_pullup = true,
  };
  esp_err_t err = i2c_new_master_bus(&bus_config, &bus);
  if (err != ESP_OK) return err;

  const i2c_device_config_t device_config = {
      .dev_addr_length = I2C_ADDR_BIT_LEN_7,
      .device_address = 0x44,
      .scl_speed_hz = 400000,
  };
  err = i2c_master_bus_add_device(bus, &device_config, &expander);
  if (err != ESP_OK) {
    i2c_del_master_bus(bus);
    return err;
  }

  // Match M5Unified's Tab5 setup for IO expander #2. P0 is WLAN_PWR_EN.
  static const uint8_t configuration[][2] = {
      {0x05, 0x81},  // OUT_SET: WLAN power on, charger enable on
      {0x03, 0xB1},  // IO_DIR
      {0x07, 0x06},  // OUT_H_IM
      {0x0D, 0x08},  // PULL_SEL
      {0x0B, 0x08},  // PULL_EN
  };
  for (size_t i = 0; i < sizeof(configuration) / sizeof(configuration[0]); ++i) {
    err = i2c_master_transmit(expander, configuration[i], sizeof(configuration[i]), 100);
    if (err != ESP_OK) break;
  }
  if (err == ESP_OK) {
    const uint8_t power_off[] = {0x05, 0x80};
    const uint8_t power_on[] = {0x05, 0x81};
    err = i2c_master_transmit(expander, power_off, sizeof(power_off), 100);
    if (err == ESP_OK) {
      vTaskDelay(pdMS_TO_TICKS(100));
      err = i2c_master_transmit(expander, power_on, sizeof(power_on), 100);
      vTaskDelay(pdMS_TO_TICKS(200));
    }
  }
  i2c_master_bus_rm_device(expander);
  i2c_del_master_bus(bus);
  if (err == ESP_OK) ESP_LOGI(TAG, "C6_BRIDGE_POWER_CYCLE ok");
  return err;
}

static bool fail(const char *stage, esp_err_t err) {
  ESP_LOGE(TAG, "C6_BRIDGE_FAIL stage=%s err=%s", stage, esp_err_to_name(err));
  return false;
}

static void report_matching_pair(const esp_hosted_coprocessor_fwver_t *version) {
  // The Windows installer starts listening after the bridge has booted. Repeat
  // the terminal result so a fast already-current boot cannot be misdiagnosed
  // as a timeout and trigger another recovery attempt.
  for (unsigned attempt = 0; attempt < 60; ++attempt) {
    ESP_LOGI(TAG, "RTL_WIFI_HOSTED host=%s coprocessor=%" PRIu32 ".%" PRIu32 ".%" PRIu32 " match=1",
             C6_HOSTED_VERSION, version->major1, version->minor1, version->patch1);
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

void app_main(void) {
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    fail("nvs_preserve", err);
    return;
  }
  if (err != ESP_OK) { fail("nvs_init", err); return; }
  if ((err = esp_event_loop_create_default()) != ESP_OK) { fail("event_loop", err); return; }
  if ((err = prepare_tab5_c6_power()) != ESP_OK) { fail("c6_power", err); return; }

  if ((err = esp_hosted_init()) != ESP_OK) { fail("hosted_init", err); return; }
  if ((err = esp_hosted_connect_to_slave()) != ESP_OK) { fail("hosted_connect", err); return; }
  esp_hosted_coprocessor_fwver_t before = {0};
  if ((err = esp_hosted_get_coprocessor_fwversion(&before)) != ESP_OK) { fail("version_before", err); return; }
  ESP_LOGI(TAG, "C6 before OTA: %" PRIu32 ".%" PRIu32 ".%" PRIu32,
           before.major1, before.minor1, before.patch1);
  char detected_version[24] = {0};
  snprintf(detected_version, sizeof(detected_version), "%" PRIu32 ".%" PRIu32 ".%" PRIu32,
           before.major1, before.minor1, before.patch1);
  if (strcmp(detected_version, C6_HOSTED_VERSION) == 0) {
    ESP_LOGI(TAG, "C6 after OTA: %s (already current)", C6_HOSTED_VERSION);
    report_matching_pair(&before);
    return;
  }
  const size_t size = esp_hosted_tab5_c6_bin_end - esp_hosted_tab5_c6_bin_start;
  if ((err = esp_hosted_slave_ota_begin()) != ESP_OK) { fail("ota_begin", err); return; }
  for (size_t offset = 0; offset < size; ) {
    const size_t chunk = size - offset > 1024 ? 1024 : size - offset;
    if ((err = esp_hosted_slave_ota_write(esp_hosted_tab5_c6_bin_start + offset, chunk)) != ESP_OK) { fail("ota_write", err); return; }
    offset += chunk;
  }
  if ((err = esp_hosted_slave_ota_end()) != ESP_OK) { fail("ota_end", err); return; }
  if ((err = esp_hosted_slave_ota_activate()) != ESP_OK) { fail("ota_activate", err); return; }
  ESP_LOGI(TAG, "C6 OTA complete: %u bytes; restarting to verify %s", (unsigned)size, C6_HOSTED_VERSION);
  esp_restart();
}
