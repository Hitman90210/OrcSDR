#pragma once

#include <cstddef>
#include <cstdint>

#include "orcsdr_storage.hpp"
#include "p25_decoder_core.hpp"

namespace orcsdr::p25config {

constexpr uint32_t kSchemaVersion = 2;
constexpr size_t kMaxControlChannels = 8;
constexpr size_t kMaxTalkgroups = 8;
constexpr size_t kMaxProfiles = 16;
constexpr char kLegacyPath[] = "/orcsdr/P25.cfg";
constexpr char kProfilesRoot[] = "/orcsdr/p25";
constexpr char kActivePath[] = "/orcsdr/p25/active.txt";
constexpr char kExportsRoot[] = "/orcsdr/exports";

struct Talkgroup {
  uint16_t id = 0;
  char alias[32]{};
};

struct Config {
  uint32_t version = kSchemaVersion;
  char system_name[48]{};
  uint16_t nac = 0;
  uint32_t wacn = 0;
  uint16_t system_id = 0;
  uint8_t rfss = 0;
  uint8_t site = 0;
  uint32_t control_channels_hz[kMaxControlChannels]{};
  uint8_t control_channel_count = 0;
  uint32_t last_control_channel_hz = 0;
  bool auto_follow = true;
  bool encryption_skip = true;
  p25core::Modulation modulation = p25core::Modulation::auto_detect;
  float cqpsk_timing_gain = 0.005f;
  float cqpsk_carrier_gain = 0.008f;
  uint16_t hold_talkgroup = 0;
  Talkgroup talkgroups[kMaxTalkgroups]{};
  uint8_t talkgroup_count = 0;
};

struct ProfileSummary {
  char id[32]{};
  char name[48]{};
};

struct StoreState {
  ProfileSummary profiles[kMaxProfiles]{};
  uint8_t count = 0;
  int8_t active_index = -1;
  char active_id[32]{};
};

enum class LoadResult : uint8_t { ok, missing, invalid, io_error };

void defaults(Config* config);
bool parse(const char* text, Config* config, char* error, size_t error_size);
bool validate(const Config& config, char* error, size_t error_size);
LoadResult load(orcsdr::storage::FileSystem& fs, const char* path, Config* config, char* error, size_t error_size);
bool save(orcsdr::storage::FileSystem& fs, const char* path, const Config& config,
          char* error, size_t error_size);
LoadResult load_active(orcsdr::storage::FileSystem& fs, Config* config,
                       StoreState* state, char* error, size_t error_size);
bool refresh(orcsdr::storage::FileSystem& fs, StoreState* state, char* error,
             size_t error_size);
bool select(orcsdr::storage::FileSystem& fs, const char* id, Config* config,
            StoreState* state, char* error, size_t error_size);
bool import_profile(orcsdr::storage::FileSystem& fs, const char* source_path,
                    const char* requested_id, StoreState* state, char* error,
                    size_t error_size);
bool export_profile(orcsdr::storage::FileSystem& fs, const char* id,
                    const char* destination_path, char* error, size_t error_size);
bool rename_profile(orcsdr::storage::FileSystem& fs, const char* id,
                    const char* name, StoreState* state, char* error,
                    size_t error_size);
bool delete_profile(orcsdr::storage::FileSystem& fs, const char* id,
                    StoreState* state, char* error, size_t error_size);
bool valid_profile_id(const char* id);
bool self_check();

}  // namespace orcsdr::p25config
