#pragma once

#include <cstddef>
#include <cstdint>
#include <esp_http_server.h>

namespace orcsdr::web_audio {
// start/stop belong to the main lifecycle owner; stop precedes httpd_stop.
bool start(httpd_handle_t server);
void stop();
bool demanded();
void publish(const int16_t* samples, size_t count);
void invalidate();
struct Counters {
  uint32_t clients, contention_samples, sent_samples, dropped_samples, send_errors;
};
Counters counters();
}
