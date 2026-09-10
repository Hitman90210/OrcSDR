#pragma once

#include <cstddef>
#include <cstdint>

namespace orcsdr::web_console {

constexpr size_t kSpectrumBins = 64;
// The device shows six aircraft because that is what its panel fits, not what
// it tracks. A browser has room for more, and the tracker holds 64.
constexpr size_t kAircraftSlots = 16;

// Range and bearing rather than latitude and longitude, deliberately. It is
// what a radar display needs, and it keeps the receiver's own position on the
// device -- absolute aircraft positions plus a centred display would give it
// away, and the console promises to leak no coordinates.
struct Aircraft {
  char label[9]{};  // callsign when known, otherwise the ICAO address
  uint16_t range_tenths_nm = 0;
  uint16_t bearing_deg = 0;
  int32_t altitude_ft = 0;
  int16_t speed_kts = 0;
  int16_t heading_deg = 0;
  int16_t vertical_rate_fpm = 0;
  int8_t signal_dbfs = 0;
  uint8_t age_seconds = 0;
  bool has_position = false;
  bool has_altitude = false;
  bool has_speed = false;
  bool has_heading = false;
};

struct Snapshot {
  char wifi_ip[16]{};
  char mode[12]{};
  char clock[12]{};
  char program_service[9]{};
  char radio_text[65]{};
  char pi_code[5]{};

  uint32_t frequency_hz = 0;
  uint32_t span_hz = 960000;
  uint32_t filter_bandwidth_hz = 0;
  uint32_t effective_sps = 0;
  int32_t battery_percent = -1;
  float signal_dbfs = -90.0f;
  float left_dbfs = -90.0f;
  float right_dbfs = -90.0f;
  // What the device itself puts on screen. The raw 0-255 value is deliberately
  // not sent: the page rendered it against "/100" once already, and half volume
  // read as "128/100".
  uint8_t volume_percent = 0;
  uint8_t spectrum[kSpectrumBins]{};
  uint8_t spectrum_count = 0;
  bool wifi_connected = false;
  bool usb_connected = false;
  bool rtl_ready = false;
  bool receiving = false;
  bool sound_enabled = true;
  bool stereo = false;
  bool rds_carrier = false;
  bool rds_locked = false;
  bool recording = false;

  // Channelised bands (CB, GMRS, marine, weather). The label is whatever the
  // device puts on its own stepper, so the two never disagree.
  char channel_label[12]{};
  uint8_t channel_index = 0;
  uint8_t channel_count = 0;
  bool channel_active = false;

  Aircraft aircraft[kAircraftSlots]{};
  uint32_t adsb_messages = 0;
  float adsb_message_rate = 0.0f;
  uint16_t radar_range_nm = 25;
  uint8_t aircraft_count = 0;    // entries filled in `aircraft`
  uint8_t aircraft_tracked = 0;  // total the receiver is tracking
  bool adsb_active = false;
  bool location_configured = false;
};

enum class CommandKind : uint8_t {
  none,
  volume_down,
  volume_up,
  sound_toggle,
  span_down,
  span_up,
  step_down,
  step_up,
  tune,
  open
};

struct Command {
  CommandKind kind = CommandKind::none;
  uint32_t value = 0;
  char id[16]{};
};

void set_enabled(bool enabled);
bool enabled();
// Control is off by default and separate from serving the page. With it off,
// POST /api/action is refused outright rather than merely hidden in the UI --
// hiding a button is not a permission.
void set_control_enabled(bool enabled);
bool control_enabled();
bool listening();
// True briefly after a browser requests an audio chunk.  This lets the SDR
// keep demodulating for remote listeners even when the local speaker is muted.
bool audio_active();
void poll(bool wifi_connected);
void update(const Snapshot& snapshot);
bool take_command(Command* command);
void tap_audio(const int16_t* samples, size_t frames, size_t stride);
void update_spectrum(const float* levels, size_t count);
size_t copy_audio(int16_t* output, size_t max_samples);
size_t copy_spectrum(uint8_t* output, size_t max_bins);
void format_url(char* out, size_t out_size, const char* ip);
bool self_check();

}  // namespace orcsdr::web_console
