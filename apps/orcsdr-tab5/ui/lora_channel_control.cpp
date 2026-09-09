#include "lora_channel_control.hpp"

#include <algorithm>
#include <cstring>
#include <iterator>

#include "nvs_store.hpp"

namespace orcsdr::lora_channel {
namespace {

// Meshtastic geographic regions whose current default/allowed profile includes
// LongFast (SF11/BW250). Specialty narrow, amateur, and 2.4 GHz plans are omitted
// because OrcSDR's native decoder or RTL-SDR hardware cannot receive them.
constexpr Region kRegions[] = {
    {"US", 902000000, 928000000},       {"EU_433", 433000000, 434000000},
    {"EU_868", 869400000, 869650000},   {"CN", 470000000, 510000000},
    {"JP", 920500000, 923500000},       {"ANZ", 915000000, 928000000},
    {"ANZ_433", 433050000, 434790000},  {"RU", 868700000, 869200000},
    {"KR", 920000000, 923000000},       {"TW", 920000000, 925000000},
    {"IN", 865000000, 867000000},       {"NZ_865", 864000000, 868000000},
    {"TH", 920000000, 925000000},       {"UA_433", 433000000, 434700000},
    {"MY_433", 433000000, 435000000},   {"MY_919", 919000000, 924000000},
    {"SG_923", 917000000, 925000000},   {"PH_433", 433000000, 434700000},
    {"PH_868", 868000000, 869400000},   {"PH_915", 915000000, 918000000},
    {"KZ_433", 433075000, 434775000},   {"KZ_863", 863000000, 868000000},
    {"NP_865", 865000000, 868000000},   {"BR_902", 902000000, 907500000},
};

constexpr ModemPreset kPresets[] = {
    {"LONGFAST", "LONG FAST", "LongFast", 250000, 11, 5},
    {"LONGTURBO", "LONG TURBO", "LongTurbo", 500000, 11, 8},
    {"SHORTTURBO", "SHORT TURBO", "ShortTurbo", 500000, 7, 5},
};
static_assert(std::size(kPresets) == static_cast<size_t>(Preset::count));

Selection current{};
bool scanning = false;
uint8_t survey_span = 0;
uint32_t survey_next_ms = 0;
uint32_t survey_restore_hz = 0;

uint32_t preset_hash(Preset value) {
  uint32_t hash = 5381;
  for (const char* c = preset(value).hash_name; *c; ++c) hash = hash * 33u + *c;
  return hash;
}

void set_selection(size_t region_index, uint16_t slot) {
  current.region_index = static_cast<uint8_t>(region_index);
  current.slot = slot == 0 ? default_slot(region_index) : slot;
  current.frequency_hz = frequency_hz(region_index, current.slot);
}

}  // namespace

size_t preset_count() { return std::size(kPresets); }

const ModemPreset& preset(Preset value) {
  const size_t index = static_cast<size_t>(value);
  return kPresets[std::min(index, std::size(kPresets) - 1)];
}

size_t region_count() { return std::size(kRegions); }

const Region& region(size_t index) {
  return kRegions[std::min(index, std::size(kRegions) - 1)];
}

int find_region(const char* code) {
  if (code == nullptr) return -1;
  for (size_t i = 0; i < std::size(kRegions); ++i)
    if (std::strcmp(kRegions[i].code, code) == 0) return static_cast<int>(i);
  return -1;
}

uint16_t slot_count(size_t region_index) {
  const Region& plan = region(region_index);
  return static_cast<uint16_t>((plan.end_hz - plan.start_hz) /
                               preset(current.preset).bandwidth_hz);
}

uint16_t default_slot(size_t region_index) {
  const uint16_t count = slot_count(region_index);
  return count == 0 ? 0 : static_cast<uint16_t>(preset_hash(current.preset) % count + 1u);
}

uint32_t frequency_hz(size_t region_index, uint16_t slot) {
  const Region& plan = region(region_index);
  slot = std::clamp<uint16_t>(slot, 1, slot_count(region_index));
  const uint32_t bandwidth_hz = preset(current.preset).bandwidth_hz;
  return plan.start_hz + bandwidth_hz / 2u +
         static_cast<uint32_t>(slot - 1u) * bandwidth_hz;
}

uint16_t slot_for_frequency(size_t region_index, uint32_t frequency) {
  const uint32_t first = frequency_hz(region_index, 1);
  if (frequency < first) return 0;
  const uint32_t offset = frequency - first;
  const uint32_t bandwidth_hz = preset(current.preset).bandwidth_hz;
  if (offset % bandwidth_hz != 0) return 0;
  const uint32_t slot = offset / bandwidth_hz + 1u;
  return slot <= slot_count(region_index) ? static_cast<uint16_t>(slot) : 0;
}

const Selection& selection() { return current; }

void load(NvsStore& store) {
  // Always leave callers with a valid plan. Previously a fresh device with no
  // LoRa keys returned with Selection{} (slot/frequency zero) until an SD
  // config happened to be loaded, which made surveys and the channel picker
  // operate on an internally invalid selection.
  const uint8_t stored_preset = store.get_u8("lora_preset", 0);
  current.preset = stored_preset < preset_count() ? static_cast<Preset>(stored_preset)
                                                  : Preset::long_fast;
  set_selection(0, default_slot(0));
  current.persisted = false;
  current.persisted = store.is_key("lora_region") && store.is_key("lora_slot");
  if (!current.persisted) return;
  const uint8_t region_index = store.get_u8("lora_region", 0);
  const uint16_t slot = store.get_u16("lora_slot", default_slot(0));
  if (region_index < region_count() && slot >= 1 && slot <= slot_count(region_index)) {
    set_selection(region_index, slot);
    return;
  }
  set_selection(0, default_slot(0));
  store.put_u8("lora_region", current.region_index);
  store.put_u16("lora_slot", current.slot);
  store.put_u8("lora_preset", static_cast<uint8_t>(current.preset));
}

bool adopt(const char* region_code, uint32_t configured_frequency_hz) {
  if (current.persisted) return false;
  int region_index = find_region(region_code);
  if (region_index < 0 && region_code != nullptr && std::strncmp(region_code, "US", 2) == 0)
    region_index = 0;
  if (region_index < 0) return false;
  const uint16_t slot = slot_for_frequency(static_cast<size_t>(region_index), configured_frequency_hz);
  if (slot == 0) return false;
  set_selection(static_cast<size_t>(region_index), slot);
  return true;
}

bool choose(size_t region_index, uint16_t slot, NvsStore& store) {
  if (region_index >= region_count()) return false;
  if (slot_count(region_index) == 0) return false;
  if (slot == 0) slot = default_slot(region_index);
  if (slot < 1 || slot > slot_count(region_index)) return false;
  set_selection(region_index, slot);
  current.persisted = store.put_u8("lora_region", current.region_index) &&
                      store.put_u16("lora_slot", current.slot) &&
                      store.put_u8("lora_preset", static_cast<uint8_t>(current.preset));
  return true;
}

bool choose_preset(Preset value, NvsStore& store) {
  if (static_cast<size_t>(value) >= preset_count()) return false;
  const Preset previous = current.preset;
  current.preset = value;
  if (slot_count(current.region_index) == 0) {
    current.preset = previous;
    return false;
  }
  set_selection(current.region_index, default_slot(current.region_index));
  current.persisted = store.put_u8("lora_region", current.region_index) &&
                      store.put_u16("lora_slot", current.slot) &&
                      store.put_u8("lora_preset", static_cast<uint8_t>(current.preset));
  return current.persisted;
}

void start_survey(uint32_t restore_frequency_hz, uint32_t now_ms) {
  survey_restore_hz = restore_frequency_hz;
  survey_span = 0;
  survey_next_ms = now_ms;
  scanning = true;
}

uint32_t cancel_survey() {
  scanning = false;
  return survey_restore_hz;
}

SurveyStep service_survey(uint32_t now_ms, bool receiver_is_lora) {
  if (!scanning || !receiver_is_lora || now_ms < survey_next_ms) return {};
  const uint8_t spans = survey_span_count();
  if (survey_span >= spans) {
    scanning = false;
    return {survey_restore_hz, survey_span, true};
  }
  const uint16_t slots = slot_count(current.region_index);
  const uint16_t slot = spans <= 1
                            ? 1
                            : static_cast<uint16_t>(
                                  1u + static_cast<uint32_t>(survey_span) * (slots - 1u) /
                                           (spans - 1u));
  ++survey_span;
  survey_next_ms = now_ms + 750;
  return {frequency_hz(current.region_index, slot), survey_span, false};
}

bool survey_active() { return scanning; }
uint8_t survey_progress() { return survey_span; }
uint8_t survey_span_count() {
  return static_cast<uint8_t>(std::min<uint16_t>(14, slot_count(current.region_index)));
}

bool self_check() {
  const Selection saved = current;
  current.preset = Preset::long_fast;
  const int us = find_region("US");
  const int eu433 = find_region("EU_433");
  const int eu868 = find_region("EU_868");
  const int ph868 = find_region("PH_868");
  const bool long_fast_ok =
         preset_count() == 3 && region_count() == 24 && us == 0 &&
         slot_count(us) == 104 && default_slot(us) == 20 &&
         frequency_hz(us, 20) == 906875000 && slot_for_frequency(us, 906875000) == 20 &&
         default_slot(eu433) == 4 && frequency_hz(eu433, 4) == 433875000 &&
         default_slot(eu868) == 1 && frequency_hz(eu868, 1) == 869525000 &&
         ph868 >= 0 && slot_count(ph868) == 5 && default_slot(ph868) == 1 &&
         frequency_hz(ph868, 1) == 868125000 && slot_for_frequency(us, 906800000) == 0;
  current.preset = Preset::long_turbo;
  const bool long_turbo_ok = slot_count(us) == 52 && default_slot(us) == 14 &&
                             frequency_hz(us, 14) == 908750000;
  current.preset = Preset::short_turbo;
  const bool short_turbo_ok = slot_count(us) == 52 && default_slot(us) == 50 &&
                              frequency_hz(us, 50) == 926750000;
  current = saved;
  return long_fast_ok && long_turbo_ok && short_turbo_ok;
}

}  // namespace orcsdr::lora_channel
