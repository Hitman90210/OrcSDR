#include "wifi_service.hpp"

#include <cstring>
#include <atomic>

extern "C" {
#include <eh_host_event.h>
#include <esp_event.h>
#include <esp_hosted.h>
#include <esp_hosted_ota.h>
#include <esp_hosted_transport_config.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
}

namespace orcsdr::wifi {
namespace {
bool g_started = false;
std::atomic<bool> g_connected{false};
std::atomic<bool> g_failed{false};
bool g_versions_match = false;
char g_c6_version[16]{"unknown"};
bool g_hosted_transport_ready = false;
portMUX_TYPE g_update_lock = portMUX_INITIALIZER_UNLOCKED;
C6UpdateStatus g_update_status{};
std::atomic<bool> g_scan_done{false};
esp_netif_t* g_sta_netif = nullptr;
const char* g_failure_stage = "none";
int32_t g_failure_code = ESP_OK;
char g_ssid[33]{};
std::atomic<uint32_t> g_ip_addr{0};
// CONFIG_ESP_HOSTED_HOST_TRANSPORT_RESTART_ON_FAILURE is deliberately off
// (see sdkconfig.defaults: auto-restart risks a reboot loop if the fault
// recurs immediately, which it likely would since it's triggered by
// sustained network traffic that would resume right after any restart).
// That leaves EH_HOST_EVENT_TRANSPORT_* unobserved anywhere in the app, so
// a real, hardware-confirmed SDIO transport fault (ESP_ERR_TIMEOUT/0x107
// between the P4 and the C6) left `connected()` reporting true throughout
// -- nothing distinguished "still associated" from "transport wedged" for
// diagnosis. This does not attempt recovery, only makes the state visible.
std::atomic<bool> g_transport_healthy{true};
std::atomic<uint32_t> g_transport_failure_count{0};
// Set by on_wifi_event() when WIFI_EVENT_STA_STOP actually arrives, cleared
// before each esp_wifi_stop() call. See reset_link()'s comment: esp_wifi_stop()
// over the hosted RPC transport is a fire-and-forget request to the C6, not a
// guarantee the stop has taken effect by the time the call returns.
std::atomic<bool> g_sta_stopped{false};

#if ORCSDR_HAS_EMBEDDED_C6_FIRMWARE
extern const uint8_t esp_hosted_tab5_c6_bin_start[]
    asm("_binary_esp_hosted_tab5_c6_bin_start");
extern const uint8_t esp_hosted_tab5_c6_bin_end[]
    asm("_binary_esp_hosted_tab5_c6_bin_end");
#endif

void set_update_status(C6UpdateState state, uint8_t progress, const char* stage) {
  portENTER_CRITICAL(&g_update_lock);
  g_update_status.state = state;
  g_update_status.progress_percent = progress;
  g_update_status.image_embedded = ORCSDR_HAS_EMBEDDED_C6_FIRMWARE;
  strlcpy(g_update_status.stage, stage ? stage : "none", sizeof(g_update_status.stage));
  portEXIT_CRITICAL(&g_update_lock);
}

void update_fail(const char* stage, esp_err_t error) {
  g_failure_stage = stage;
  g_failure_code = error;
  set_update_status(C6UpdateState::failed, 0, stage);
  ESP_LOGE("orcsdr_wifi", "C6 update failed stage=%s err=%s", stage, esp_err_to_name(error));
  ESP_LOGE("orcsdr_wifi", "RTL_WIFI_C6_UPDATE state=failed stage=%s err=%s", stage,
           esp_err_to_name(error));
}

void c6_update_task(void*) {
#if ORCSDR_HAS_EMBEDDED_C6_FIRMWARE
  const size_t size = esp_hosted_tab5_c6_bin_end - esp_hosted_tab5_c6_bin_start;
  if (size == 0) { update_fail("image_empty", ESP_ERR_INVALID_SIZE); vTaskDelete(nullptr); return; }
  if (const esp_err_t result = esp_hosted_slave_ota_begin(); result != ESP_OK) {
    update_fail("ota_begin", result); vTaskDelete(nullptr); return;
  }
  uint8_t reported = 0;
  for (size_t offset = 0; offset < size;) {
    const size_t chunk = size - offset > 1024 ? 1024 : size - offset;
    if (const esp_err_t result = esp_hosted_slave_ota_write(esp_hosted_tab5_c6_bin_start + offset, chunk);
        result != ESP_OK) {
      (void)esp_hosted_slave_ota_end();
      update_fail("ota_write", result); vTaskDelete(nullptr); return;
    }
    offset += chunk;
    const uint8_t progress = static_cast<uint8_t>((offset * 100u) / size);
    set_update_status(C6UpdateState::updating, progress, "writing");
    if (progress >= reported + 10 || progress == 100) {
      reported = progress;
      ESP_LOGI("orcsdr_wifi", "RTL_WIFI_C6_UPDATE state=writing percent=%u", progress);
    }
    vTaskDelay(1);
  }
  if (const esp_err_t result = esp_hosted_slave_ota_end(); result != ESP_OK) {
    update_fail("ota_end", result); vTaskDelete(nullptr); return;
  }
  if (const esp_err_t result = esp_hosted_slave_ota_activate(); result != ESP_OK) {
    update_fail("ota_activate", result); vTaskDelete(nullptr); return;
  }
  set_update_status(C6UpdateState::rebooting, 100, "restarting");
  ESP_LOGI("orcsdr_wifi", "RTL_WIFI_C6_UPDATE state=rebooting target=%s bytes=%u",
           ORCSDR_HOSTED_C6_VERSION, static_cast<unsigned>(size));
  vTaskDelay(pdMS_TO_TICKS(300));
  esp_restart();
#else
  update_fail("image_unavailable", ESP_ERR_NOT_SUPPORTED);
#endif
  vTaskDelete(nullptr);
}

const char* security_name(wifi_auth_mode_t authmode) {
  switch (authmode) {
    case WIFI_AUTH_OPEN: return "OPEN";
    case WIFI_AUTH_WEP: return "WEP";
    case WIFI_AUTH_WPA_PSK: return "WPA";
    case WIFI_AUTH_WPA2_PSK: return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK: return "WPA/WPA2";
    case WIFI_AUTH_ENTERPRISE: return "WPA2-ENT";
    case WIFI_AUTH_WPA3_PSK: return "WPA3";
    case WIFI_AUTH_WPA2_WPA3_PSK: return "WPA2/WPA3";
    case WIFI_AUTH_WAPI_PSK: return "WAPI";
    case WIFI_AUTH_OWE: return "OWE";
    case WIFI_AUTH_WPA3_ENT_192: return "WPA3-ENT";
    case WIFI_AUTH_DPP: return "DPP";
    case WIFI_AUTH_WPA3_ENTERPRISE: return "WPA3-ENT";
    case WIFI_AUTH_WPA2_WPA3_ENTERPRISE: return "WPA2/3-ENT";
    case WIFI_AUTH_WPA_ENTERPRISE: return "WPA-ENT";
    default: return "UNKNOWN";
  }
}

void format_phy(const wifi_ap_record_t& record, char* out, size_t size) {
  char* cursor = out;
  size_t remaining = size;
  const auto append = [&](const char* value) {
    const int written = snprintf(cursor, remaining, "%s%s", cursor == out ? "" : "/", value);
    if (written > 0 && static_cast<size_t>(written) < remaining) {
      cursor += written;
      remaining -= static_cast<size_t>(written);
    }
  };
  if (record.phy_11b) append("b");
  if (record.phy_11g) append("g");
  if (record.phy_11n) append("n");
  if (record.phy_11ax) append("ax");
  if (cursor == out) strlcpy(out, "unknown", size);
}

std::atomic<uint8_t> g_disconnect_reason{0};

void on_wifi_event(void*, esp_event_base_t base, int32_t id, void* data) {
  if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
    const auto* event = static_cast<const wifi_event_sta_disconnected_t*>(data);
    const uint8_t reason = event ? event->reason : 0u;
    ESP_LOGW("orcsdr_wifi", "station disconnected reason=%u", reason);
    // Kept so the UI can say *why* the link dropped: "it just disconnects" is
    // a very different problem for reason 15 (bad key) than for 200/201
    // (beacon timeout / no AP found), and the user cannot tell them apart.
    g_disconnect_reason.store(reason, std::memory_order_release);
    g_connected.store(false, std::memory_order_release);
    g_failed.store(true, std::memory_order_release);
  }
  if (base == WIFI_EVENT && id == WIFI_EVENT_SCAN_DONE) {
    g_scan_done.store(true, std::memory_order_release);
    ESP_LOGI("orcsdr_wifi", "scan done event");
  }
  if (base == WIFI_EVENT && id == WIFI_EVENT_STA_STOP) {
    g_sta_stopped.store(true, std::memory_order_release);
    ESP_LOGI("orcsdr_wifi", "station stop confirmed");
  }
}

void on_ip_event(void*, esp_event_base_t, int32_t, void* data) {
  const auto* event = static_cast<ip_event_got_ip_t*>(data);
  g_ip_addr.store(event->ip_info.ip.addr, std::memory_order_release);
  g_failed.store(false, std::memory_order_release);
  g_connected.store(true, std::memory_order_release);
}

void on_transport_event(void*, esp_event_base_t, int32_t id, void*) {
  if (id == EH_HOST_EVENT_TRANSPORT_DOWN) {
    g_transport_healthy.store(false, std::memory_order_release);
    ESP_LOGW("orcsdr_wifi", "RTL_WIFI_TRANSPORT_DOWN");
  } else if (id == EH_HOST_EVENT_TRANSPORT_UP) {
    g_transport_healthy.store(true, std::memory_order_release);
    ESP_LOGI("orcsdr_wifi", "RTL_WIFI_TRANSPORT_UP");
  } else if (id == EH_HOST_EVENT_TRANSPORT_FAILURE) {
    g_transport_healthy.store(false, std::memory_order_release);
    const uint32_t count = g_transport_failure_count.fetch_add(1, std::memory_order_relaxed) + 1;
    ESP_LOGE("orcsdr_wifi", "RTL_WIFI_TRANSPORT_FAILURE count=%lu",
             static_cast<unsigned long>(count));
  }
}
}

bool start() {
  if (g_started) return true;
  g_failure_stage = "none";
  g_failure_code = ESP_OK;
  g_versions_match = false;
  g_hosted_transport_ready = false;
  strlcpy(g_c6_version, "unknown", sizeof(g_c6_version));
  const esp_err_t netif = esp_netif_init();
  if (netif != ESP_OK && netif != ESP_ERR_INVALID_STATE) {
    g_failure_stage = "netif"; g_failure_code = netif; return false;
  }
  const esp_err_t loop = esp_event_loop_create_default();
  if (loop != ESP_OK && loop != ESP_ERR_INVALID_STATE) {
    g_failure_stage = "event_loop"; g_failure_code = loop; return false;
  }
  if (g_sta_netif == nullptr) {
    g_sta_netif = esp_netif_create_default_wifi_sta();
    if (g_sta_netif == nullptr) { g_failure_stage = "sta_netif"; g_failure_code = ESP_FAIL; return false; }
  }
  const esp_err_t hosted_init = esp_hosted_init();
  if (hosted_init != ESP_OK) {
    g_failure_stage = "hosted_init"; g_failure_code = hosted_init;
    set_update_status(C6UpdateState::unreachable, 0, "hosted_init"); return false;
  }
  const esp_err_t hosted_connect = esp_hosted_connect_to_slave();
  if (hosted_connect != ESP_OK) {
    g_failure_stage = "hosted_connect"; g_failure_code = hosted_connect;
    set_update_status(C6UpdateState::unreachable, 0, "hosted_connect"); return false;
  }
  g_hosted_transport_ready = true;
  esp_hosted_coprocessor_fwver_t cp{};
  const esp_err_t version = esp_hosted_get_coprocessor_fwversion(&cp);
  if (version == ESP_OK)
    snprintf(g_c6_version, sizeof(g_c6_version), "%lu.%lu.%lu",
             static_cast<unsigned long>(cp.major1), static_cast<unsigned long>(cp.minor1),
             static_cast<unsigned long>(cp.patch1));
  else
    strlcpy(g_c6_version, "unavailable", sizeof(g_c6_version));
  g_versions_match = version == ESP_OK &&
                     cp.major1 == 3 && cp.minor1 == 0 && cp.patch1 == 6;
  if (!g_versions_match) {
    g_failure_stage = "version"; g_failure_code = version;
    set_update_status(version == ESP_OK && ORCSDR_HAS_EMBEDDED_C6_FIRMWARE
                          ? C6UpdateState::ready : C6UpdateState::unavailable,
                      0, version == ESP_OK ? "confirm_required" : "version_query");
    return false;
  }
  set_update_status(C6UpdateState::current, 100, "current");
  wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
  const esp_err_t wifi_init = esp_wifi_init(&wifi_cfg);
  if (wifi_init != ESP_OK && wifi_init != ESP_ERR_WIFI_INIT_STATE) {
    g_failure_stage = "wifi_init"; g_failure_code = wifi_init; return false;
  }
  const esp_err_t wifi_event = esp_event_handler_register(
      WIFI_EVENT, ESP_EVENT_ANY_ID, on_wifi_event, nullptr);
  if (wifi_event != ESP_OK) { g_failure_stage = "wifi_event"; g_failure_code = wifi_event; return false; }
  const esp_err_t ip_event = esp_event_handler_register(
      IP_EVENT, IP_EVENT_STA_GOT_IP, on_ip_event, nullptr);
  if (ip_event != ESP_OK) { g_failure_stage = "ip_event"; g_failure_code = ip_event; return false; }
  const esp_err_t transport_event = esp_event_handler_register(
      EH_HOST_EVENT, ESP_EVENT_ANY_ID, on_transport_event, nullptr);
  if (transport_event != ESP_OK) {
    g_failure_stage = "transport_event"; g_failure_code = transport_event; return false;
  }
  const esp_err_t wifi_mode = esp_wifi_set_mode(WIFI_MODE_STA);
  if (wifi_mode != ESP_OK) { g_failure_stage = "wifi_mode"; g_failure_code = wifi_mode; return false; }
  const esp_err_t wifi_start = esp_wifi_start();
  if (wifi_start != ESP_OK) { g_failure_stage = "wifi_start"; g_failure_code = wifi_start; return false; }
  // ESP-IDF's WiFi station defaults to WIFI_PS_MIN_MODEM (radio sleeps
  // between DTIM beacons) unless told otherwise -- this app never had.
  // Disabling it was tested on hardware as a candidate fix for the
  // ESP_ERR_TIMEOUT/0x107 SDIO transport fault (see wifi_service.hpp's
  // transport_healthy comment) and did NOT eliminate it -- the flood
  // reproduced identically with this in place, so power-save was not the
  // cause. Kept anyway: Espressif's own esp_hosted iperf example disables
  // it for sustained throughput, it's harmless, and non-fatal here (WiFi
  // still works with default power-save if this call ever fails).
  const esp_err_t ps_result = esp_wifi_set_ps(WIFI_PS_NONE);
  if (ps_result != ESP_OK) {
    ESP_LOGW("orcsdr_wifi", "esp_wifi_set_ps(WIFI_PS_NONE) failed: 0x%x",
             static_cast<unsigned>(ps_result));
  }
  g_started = true;
  return true;
}

void stop() { if (g_started) { esp_wifi_disconnect(); esp_wifi_stop(); } g_started = false; g_connected.store(false, std::memory_order_release); }

// Manual recovery for a wedged SDIO transport (see transport_healthy()).
// Deliberately does NOT call stop()/start(): those toggle g_started, and
// start() unconditionally calls esp_event_handler_register() for
// WIFI_EVENT/IP_EVENT/EH_HOST_EVENT -- fine once at boot, but calling it a
// second time here would register the same handlers again, so every event
// would fire the app's handlers twice for the rest of the session. This
// cycles only the layers that actually need a fresh start (Wi-Fi driver +
// esp_hosted transport), reusing the handlers already registered once.
//
// A first version of this function called esp_wifi_stop() and immediately
// followed it with esp_hosted_deinit(). esp_wifi_stop() over the hosted RPC
// transport is a fire-and-forget request to the C6 -- it returns once the
// request is sent, not once the C6 has actually applied it and reported
// WIFI_EVENT_STA_STOP back. Tearing down the transport (and the RPC
// event-handler registration that would deliver that confirmation) right
// after meant the C6's WiFi never actually stopped: hardware-confirmed, this
// crashed with "assert failed: netif_add (netif already added)", because the
// still-running station reported a fresh WIFI_EVENT_STA_START once the
// transport reconnected, and the explicit esp_wifi_start() below produced a
// second one -- two STA_START events with no STA_STOP between them.
// esp_netif's default STA_START handler calls netif_add() unconditionally
// (confirmed in ESP-IDF's esp_netif_lwip.c: esp_netif_start_api() has no
// guard against an already-registered netif), so the second one crashed.
// Waiting here for the real STA_STOP before touching the transport is the
// fix. If it doesn't arrive -- plausible if the transport is wedged badly
// enough that even a stop request can't complete -- bail out without
// touching esp_hosted at all rather than risk the same crash again.
bool reset_link() {
  if (!g_started) return false;
  ESP_LOGW("orcsdr_wifi", "RTL_WIFI_RESET_LINK requested");
  g_sta_stopped.store(false, std::memory_order_release);
  esp_wifi_disconnect();
  g_connected.store(false, std::memory_order_release);
  esp_err_t err = esp_wifi_stop();
  if (err != ESP_OK) {
    ESP_LOGE("orcsdr_wifi", "reset_link: esp_wifi_stop failed: 0x%x", static_cast<unsigned>(err));
    return false;
  }
  constexpr uint32_t kStopTimeoutMs = 3000;
  constexpr uint32_t kPollMs = 50;
  uint32_t waited_ms = 0;
  while (!g_sta_stopped.load(std::memory_order_acquire) && waited_ms < kStopTimeoutMs) {
    vTaskDelay(pdMS_TO_TICKS(kPollMs));
    waited_ms += kPollMs;
  }
  if (!g_sta_stopped.load(std::memory_order_acquire)) {
    ESP_LOGE("orcsdr_wifi",
             "reset_link: STA_STOP not confirmed after %ums, aborting without touching "
             "the transport",
             static_cast<unsigned>(kStopTimeoutMs));
    return false;
  }
  g_hosted_transport_ready = false;
  err = esp_hosted_deinit();
  if (err != ESP_OK) {
    ESP_LOGE("orcsdr_wifi", "reset_link: esp_hosted_deinit failed: 0x%x", static_cast<unsigned>(err));
    return false;
  }
  err = esp_hosted_init();
  if (err != ESP_OK) {
    ESP_LOGE("orcsdr_wifi", "reset_link: esp_hosted_init failed: 0x%x", static_cast<unsigned>(err));
    return false;
  }
  err = esp_hosted_connect_to_slave();
  if (err != ESP_OK) {
    ESP_LOGE("orcsdr_wifi", "reset_link: esp_hosted_connect_to_slave failed: 0x%x",
             static_cast<unsigned>(err));
    return false;
  }
  g_hosted_transport_ready = true;
  err = esp_wifi_set_mode(WIFI_MODE_STA);
  if (err != ESP_OK)
    ESP_LOGW("orcsdr_wifi", "reset_link: esp_wifi_set_mode: 0x%x", static_cast<unsigned>(err));
  err = esp_wifi_start();
  if (err != ESP_OK) {
    ESP_LOGE("orcsdr_wifi", "reset_link: esp_wifi_start failed: 0x%x", static_cast<unsigned>(err));
    return false;
  }
  esp_wifi_set_ps(WIFI_PS_NONE);
  ESP_LOGW("orcsdr_wifi", "RTL_WIFI_RESET_LINK succeeded");
  return true;
}

uint8_t last_disconnect_reason() {
  return g_disconnect_reason.load(std::memory_order_acquire);
}

const char* disconnect_reason_text(uint8_t reason) {
  switch (reason) {
    case 1: return "unspecified";
    case 2: return "auth expired";
    case 4: return "inactivity timeout";
    case 5: return "AP is full";
    case 8: return "deauth by AP";
    case 15: return "wrong password";
    case 39: return "timeout";
    case 200: return "beacon timeout";
    case 201: return "network not found";
    case 202: return "auth failed";
    case 203: return "handshake failed";
    case 204: return "handshake timeout";
    case 205: return "connection lost";
    default: return "";
  }
}

bool begin_scan() {
  if (!g_started) return false;
  g_scan_done.store(false, std::memory_order_release);
  const esp_err_t result = esp_wifi_scan_start(nullptr, false);
  if (result == ESP_OK) return true;
  ESP_LOGE("orcsdr_wifi", "scan start failed: %s", esp_err_to_name(result));
  return false;
}
int scan_results(ScanResult* results, size_t capacity) {
  uint16_t count = 0;
  if (!g_started || !g_scan_done.load(std::memory_order_acquire) ||
      esp_wifi_scan_get_ap_num(&count) != ESP_OK) return -1;
  if (results == nullptr || capacity == 0) return count;
  const uint16_t take = static_cast<uint16_t>(count < capacity ? count : capacity);
  // Deliberately on the stack, not static: internal RAM is far scarcer than
  // main-task stack here. Making this (and the ScanResult array in
  // poll_wifi()) static moved ~5KB into .bss, dropped free internal RAM from
  // 43KB to 40KB, and boot-looped the device -- app_main aborts with
  // ESP_ERR_NO_MEM when it cannot reserve its 40K internal/DMA pool.
  wifi_ap_record_t records[16]{};
  uint16_t received = take < 16 ? take : 16;
  if (esp_wifi_scan_get_ap_records(&received, records) != ESP_OK) return -1;
  g_scan_done.store(false, std::memory_order_release);
  for (uint16_t i = 0; i < received; ++i) {
    strlcpy(results[i].ssid, reinterpret_cast<const char*>(records[i].ssid), sizeof(results[i].ssid));
    memcpy(results[i].bssid, records[i].bssid, sizeof(results[i].bssid));
    results[i].rssi = records[i].rssi;
    results[i].channel = records[i].primary;
    results[i].secondary_channel_offset = records[i].second == WIFI_SECOND_CHAN_ABOVE ? 4 :
                                          records[i].second == WIFI_SECOND_CHAN_BELOW ? -4 : 0;
    strlcpy(results[i].security, security_name(records[i].authmode), sizeof(results[i].security));
    format_phy(records[i], results[i].phy, sizeof(results[i].phy));
    results[i].secure = records[i].authmode != WIFI_AUTH_OPEN;
  }
  return received;
}
bool connect(const char* network, const char* password) {
  if (!g_started || !network || !*network) return false;
  wifi_config_t config{};
  strlcpy(reinterpret_cast<char*>(config.sta.ssid), network, sizeof(config.sta.ssid));
  strlcpy(reinterpret_cast<char*>(config.sta.password), password ? password : "", sizeof(config.sta.password));
  g_failed.store(false, std::memory_order_release);
  g_connected.store(false, std::memory_order_release);
  strlcpy(g_ssid, network, sizeof(g_ssid));
  const esp_err_t set_config = esp_wifi_set_config(WIFI_IF_STA, &config);
  if (set_config != ESP_OK) {
    ESP_LOGE("orcsdr_wifi", "set_config failed: 0x%x", static_cast<unsigned>(set_config));
    return false;
  }
  const esp_err_t connect = esp_wifi_connect();
  if (connect != ESP_OK)
    ESP_LOGE("orcsdr_wifi", "connect request failed: 0x%x", static_cast<unsigned>(connect));
  return connect == ESP_OK;
}
void disconnect() { esp_wifi_disconnect(); g_connected.store(false, std::memory_order_release); }
bool connected() { return g_connected.load(std::memory_order_acquire); }
bool connect_failed() { return g_failed.load(std::memory_order_acquire); }
// Separate from connected(): the SDIO link to the C6 can wedge (see the
// on_transport_event comment above) while the higher-level station stays
// "associated", so connected() alone doesn't tell you the transport is
// actually able to move data right now.
bool transport_healthy() { return g_transport_healthy.load(std::memory_order_acquire); }
uint32_t transport_failure_count() { return g_transport_failure_count.load(std::memory_order_relaxed); }
// EH_HOST_EVENT_TRANSPORT_UP (the only thing that would normally clear
// g_transport_healthy) is posted solely from eh_host_connect_to_slave()'s
// initial bring-up -- confirmed by reading eh_host_core.c -- never after a
// transient SDIO wedge clears on its own mid-session. Left alone,
// transport_healthy() would report unhealthy for the rest of the boot after
// the FIRST fault even once the link recovers (hardware-observed: a wedge
// that flooded ESP_ERR_TIMEOUT for 20+ seconds went completely silent
// afterward with no TRANSPORT_UP event). A caller that just completed a
// full HTTPS round trip after an earlier failure has direct proof the link
// is working again, so it can call this to correct the flag itself.
void note_transport_recovered() { g_transport_healthy.store(true, std::memory_order_release); }
const char* ssid() { return g_ssid; }
const char* ip() {
  static char snapshot[16];
  esp_ip4_addr_t address{};
  address.addr = g_ip_addr.load(std::memory_order_acquire);
  snprintf(snapshot, sizeof(snapshot), IPSTR, IP2STR(&address));
  return snapshot;
}
// esp_wifi_sta_get_ap_info() is an RPC round trip to the C6 (rpc_v2
// Req_WifiStaGetApInfo, msg_id 294), not a local register read, and it blocks
// the caller until the C6 answers or the 5s RPC timeout expires. Its only
// caller is device_status::collect(), which the Home dashboard refreshes at
// 2 Hz -- so while the SDIO transport was wedged, the UI thread blocked for
// 5s at a time, twice a second. Hardware-confirmed: every "RTL_MAIN_STALL
// stage=loop_gap elapsed_ms=5000+" seen in testing paired with an
// "eh_host_feat_rpc: request: no response ... msg_id=294 (5000 ms)" warning
// immediately before it -- the wedge itself doesn't freeze the UI, this poll
// blocking on it does.
//
// RSSI is a cosmetic readout, so serve it from cache, refresh it far less
// often than the UI repaints, and don't issue the call at all while the
// transport is known-wedged. The rarer probe interval while unhealthy still
// lets a link that recovered on its own get noticed: a completed round trip
// is direct proof the transport works, so it clears the sticky unhealthy
// flag the same way a successful HTTPS fetch does.
int16_t rssi() {
  constexpr uint32_t kRefreshMs = 5000;
  constexpr uint32_t kProbeWhileWedgedMs = 30000;
  static int16_t cached = 0;
  static uint32_t last_ms = 0;
  const uint32_t now_ms = static_cast<uint32_t>(xTaskGetTickCount()) * portTICK_PERIOD_MS;
  const bool healthy = g_transport_healthy.load(std::memory_order_acquire);
  if (last_ms != 0 && now_ms - last_ms < (healthy ? kRefreshMs : kProbeWhileWedgedMs))
    return cached;
  last_ms = now_ms;
  wifi_ap_record_t ap{};
  if (esp_wifi_sta_get_ap_info(&ap) != ESP_OK) return cached;
  cached = ap.rssi;
  if (!healthy) note_transport_recovered();
  return cached;
}
bool hosted_versions_match() { return g_versions_match; }
const char* hosted_c6_version() { return g_c6_version; }
bool hosted_transport_ready() { return g_hosted_transport_ready; }
const char* hosted_failure_stage() { return g_failure_stage; }
int32_t hosted_failure_code() { return g_failure_code; }
C6UpdateStatus c6_update_status() {
  C6UpdateStatus snapshot{};
  portENTER_CRITICAL(&g_update_lock);
  snapshot = g_update_status;
  portEXIT_CRITICAL(&g_update_lock);
  return snapshot;
}
const char* c6_update_state_name(C6UpdateState state) {
  switch (state) {
    case C6UpdateState::unreachable: return "unreachable";
    case C6UpdateState::current: return "current";
    case C6UpdateState::ready: return "ready";
    case C6UpdateState::updating: return "updating";
    case C6UpdateState::failed: return "failed";
    case C6UpdateState::rebooting: return "rebooting";
    default: return "unavailable";
  }
}
bool begin_c6_update() {
  const C6UpdateStatus status = c6_update_status();
  if (!g_hosted_transport_ready || !status.image_embedded || status.state != C6UpdateState::ready)
    return false;
  set_update_status(C6UpdateState::updating, 0, "starting");
  if (xTaskCreate(c6_update_task, "c6_ota", 4096, nullptr, 4, nullptr) != pdPASS) {
    update_fail("task_create", ESP_ERR_NO_MEM);
    return false;
  }
  ESP_LOGI("orcsdr_wifi", "RTL_WIFI_C6_UPDATE state=starting target=%s",
           ORCSDR_HOSTED_C6_VERSION);
  return true;
}
}  // namespace orcsdr::wifi
