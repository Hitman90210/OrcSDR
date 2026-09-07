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
