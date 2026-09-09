#pragma once

#include <cstddef>
#include <cstdint>

namespace orcsdr {
class NvsStore;
}

namespace orcsdr::lora_channel {

enum class Preset : uint8_t { long_fast, long_turbo, short_turbo, count };

struct ModemPreset {
  const char* code;
  const char* label;
  const char* hash_name;
  uint32_t bandwidth_hz;
  uint8_t spreading_factor;
  uint8_t coding_rate;
};

struct Region {
  const char* code;
  uint32_t start_hz;
  uint32_t end_hz;
};

struct Selection {
  uint8_t region_index = 0;
  Preset preset = Preset::long_fast;
  uint16_t slot = 20;
  uint32_t frequency_hz = 906875000;
  bool persisted = false;
};

struct SurveyStep {
  uint32_t frequency_hz = 0;
  uint8_t span = 0;
  bool restore = false;
};

constexpr uint32_t kLongFastBandwidthHz = 250000;

size_t preset_count();
const ModemPreset& preset(Preset value);
size_t region_count();
const Region& region(size_t index);
int find_region(const char* code);
uint16_t slot_count(size_t region_index);
uint16_t default_slot(size_t region_index);
uint32_t frequency_hz(size_t region_index, uint16_t slot);
uint16_t slot_for_frequency(size_t region_index, uint32_t frequency_hz);

const Selection& selection();
void load(NvsStore& store);
bool adopt(const char* region_code, uint32_t frequency_hz);
bool choose(size_t region_index, uint16_t slot, NvsStore& store);
bool choose_preset(Preset value, NvsStore& store);

void start_survey(uint32_t restore_frequency_hz, uint32_t now_ms);
uint32_t cancel_survey();
SurveyStep service_survey(uint32_t now_ms, bool receiver_is_lora);
bool survey_active();
uint8_t survey_progress();
uint8_t survey_span_count();

bool self_check();

}  // namespace orcsdr::lora_channel
