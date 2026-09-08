#pragma once

#include <cstddef>
#include <cstdint>

namespace orcsdr::wifi {

struct ScanResult {
  char ssid[33]{};
  uint8_t bssid[6]{};
  int16_t rssi = 0;
  uint8_t channel = 0;
  int8_t secondary_channel_offset = 0;
  char security[24]{};
  char phy[12]{};
  bool secure = false;
};

enum class C6UpdateState : uint8_t { unavailable, unreachable, current, ready, updating, failed, rebooting };

struct C6UpdateStatus {
  C6UpdateState state = C6UpdateState::unavailable;
  uint8_t progress_percent = 0;
  bool image_embedded = false;
  char stage[24]{};
};

bool start();
void stop();
// Reason code from the most recent WIFI_EVENT_STA_DISCONNECTED (0 if none),
// and a short human-readable form ("" when the code has no friendly name).
uint8_t last_disconnect_reason();
const char* disconnect_reason_text(uint8_t reason);

bool begin_scan();
int scan_results(ScanResult* results, size_t capacity);
bool connect(const char* ssid, const char* password);
void disconnect();
bool connected();
bool connect_failed();
// True unless the SDIO transport to the C6 co-processor has wedged (a real,
// hardware-confirmed fault distinct from Wi-Fi association -- see
// wifi_service.cpp's on_transport_event). Auto-restart-on-failure is
// deliberately disabled, so this only reports the state; it doesn't recover.
bool transport_healthy();
uint32_t transport_failure_count();
// Corrects transport_healthy() after a caller proves the link actually
// works again (e.g. a retried HTTPS fetch just succeeded) -- the driver
// itself never re-fires TRANSPORT_UP for a mid-session recovery, only for
// initial bring-up. See wifi_service.cpp's definition for detail.
void note_transport_recovered();
// Manual recovery for a wedged SDIO transport (see transport_healthy()):
// tears down and rebuilds the esp_hosted link (deinit/init/reconnect) and
// the Wi-Fi driver on top of it, without touching the deliberately-disabled
// CONFIG_ESP_HOSTED_HOST_TRANSPORT_RESTART_ON_FAILURE path or rebooting the
// P4. Only meaningful while start() has already succeeded once. Blocks the
// caller for the duration (same bring-up work boot already does, plus up to
// 3s waiting for the Wi-Fi driver to confirm it actually stopped before
// touching the transport -- see wifi_service.cpp's definition for why),
// so call it from a context that can afford a brief pause, not a tight
// loop. Does not reconnect to a station itself -- the caller re-associates
// afterward.
bool reset_link();
const char* ssid();
const char* ip();
int16_t rssi();
bool hosted_versions_match();
const char* hosted_c6_version();
bool hosted_transport_ready();
const char* hosted_failure_stage();
int32_t hosted_failure_code();
C6UpdateStatus c6_update_status();
const char* c6_update_state_name(C6UpdateState state);
bool begin_c6_update();

}  // namespace orcsdr::wifi
