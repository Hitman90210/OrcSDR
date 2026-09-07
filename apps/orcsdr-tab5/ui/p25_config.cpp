#include "p25_config.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>

namespace orcsdr::p25config {
namespace {

constexpr uint32_t kP25MinHz = 24000000;
constexpr uint32_t kP25MaxHz = 1766000000;

bool profile_path(const char* id, char* path, size_t size) {
  if (!valid_profile_id(id) || path == nullptr || size == 0) return false;
  return snprintf(path, size, "%s/%s/profile.cfg", kProfilesRoot, id) > 0;
}

bool write_active(orcsdr::storage::FileSystem& fs, const char* id) {
  if (!valid_profile_id(id)) return false;
  char temporary[64]{}, backup[64]{};
  snprintf(temporary, sizeof(temporary), "%s.part", kActivePath);
  snprintf(backup, sizeof(backup), "%s.bak", kActivePath);
  fs.remove(temporary);
  File file = fs.open(temporary, FILE_WRITE, true);
  if (!file || file.printf("%s\n", id) == 0) return false;
  file.close();
  fs.remove(backup);
  const bool had_active = fs.exists(kActivePath);
  if (had_active && !fs.rename(kActivePath, backup)) return false;
  if (!fs.rename(temporary, kActivePath)) {
    if (had_active) fs.rename(backup, kActivePath);
    return false;
  }
  fs.remove(backup);
  return true;
}

void set_error(char* error, size_t size, const char* value) {
  if (error == nullptr || size == 0) return;
  snprintf(error, size, "%s", value);
}

char* trim(char* value) {
  while (*value && isspace(static_cast<unsigned char>(*value))) ++value;
  char* end = value + strlen(value);
  while (end > value && isspace(static_cast<unsigned char>(end[-1]))) --end;
  *end = '\0';
  return value;
}

bool parse_uint(const char* value, uint32_t* output) {
  if (value == nullptr || *value == '\0') return false;
  char* end = nullptr;
  const unsigned long parsed = strtoul(value, &end, 10);
  if (*end != '\0' || parsed > UINT32_MAX) return false;
  *output = static_cast<uint32_t>(parsed);
  return true;
}

bool parse_bool(const char* value, bool* output) {
  if (strcmp(value, "true") == 0 || strcmp(value, "1") == 0) {
    *output = true;
    return true;
  }
  if (strcmp(value, "false") == 0 || strcmp(value, "0") == 0) {
    *output = false;
    return true;
  }
  return false;
}

bool parse_float(const char* value, float* output) {
  char* end = nullptr;
  const float parsed = strtof(value, &end);
  if (value == end || *end != '\0' || !std::isfinite(parsed)) return false;
  *output = parsed;
  return true;
}

bool has_channel(const Config& config, uint32_t frequency_hz) {
  for (size_t i = 0; i < config.control_channel_count; ++i)
    if (config.control_channels_hz[i] == frequency_hz) return true;
  return false;
}

bool has_talkgroup(const Config& config, uint16_t id) {
  for (size_t i = 0; i < config.talkgroup_count; ++i)
    if (config.talkgroups[i].id == id) return true;
  return false;
}

bool parse_text_impl(const char* text, Config* config, char* error, size_t error_size) {
  Config parsed{};
  snprintf(parsed.system_name, sizeof(parsed.system_name), "%s", "My P25 System");
  bool has_version = false;
  uint32_t line_number = 0;
  const char* cursor = text;
  while (*cursor) {
    ++line_number;
    char line[128]{};
    size_t length = 0;
    while (*cursor && *cursor != '\n' && length + 1 < sizeof(line)) line[length++] = *cursor++;
    if (*cursor == '\n') ++cursor;
    if (*cursor && length + 1 == sizeof(line)) {
      set_error(error, error_size, "line too long");
      return false;
    }
    char* value = trim(line);
    if (*value == '\0' || *value == '#' || *value == ';') continue;
    char* equals = strchr(value, '=');
    if (equals == nullptr) {
      snprintf(error, error_size, "line %lu missing =", static_cast<unsigned long>(line_number));
      return false;
    }
    *equals = '\0';
    char* key = trim(value);
    char* field = trim(equals + 1);
    uint32_t number = 0;
    if (strcmp(key, "version") == 0) {
      if (!parse_uint(field, &number) || (number != 1 && number != kSchemaVersion)) {
        snprintf(error, error_size, "line %lu version", static_cast<unsigned long>(line_number));
        return false;
      }
      parsed.version = kSchemaVersion;
      has_version = true;
    } else if (strcmp(key, "system_name") == 0) {
      if (*field == '\0' || strlen(field) >= sizeof(parsed.system_name)) {
        snprintf(error, error_size, "line %lu system_name", static_cast<unsigned long>(line_number));
        return false;
      }
      snprintf(parsed.system_name, sizeof(parsed.system_name), "%s", field);
    } else if (strcmp(key, "nac") == 0) {
      if (!parse_uint(field, &number) || number > 0xFFF) {
        snprintf(error, error_size, "line %lu nac", static_cast<unsigned long>(line_number)); return false;
      }
      parsed.nac = static_cast<uint16_t>(number);
    } else if (strcmp(key, "wacn") == 0) {
      if (!parse_uint(field, &number) || number > 0xFFFFF) {
        snprintf(error, error_size, "line %lu wacn", static_cast<unsigned long>(line_number)); return false;
      }
      parsed.wacn = number;
    } else if (strcmp(key, "system_id") == 0) {
      if (!parse_uint(field, &number) || number > 0xFFF) {
        snprintf(error, error_size, "line %lu system_id", static_cast<unsigned long>(line_number)); return false;
      }
      parsed.system_id = static_cast<uint16_t>(number);
    } else if (strcmp(key, "rfss") == 0) {
      if (!parse_uint(field, &number) || number > UINT8_MAX) {
        snprintf(error, error_size, "line %lu rfss", static_cast<unsigned long>(line_number)); return false;
      }
      parsed.rfss = static_cast<uint8_t>(number);
    } else if (strcmp(key, "site") == 0) {
      if (!parse_uint(field, &number) || number > UINT8_MAX) {
        snprintf(error, error_size, "line %lu site", static_cast<unsigned long>(line_number)); return false;
      }
      parsed.site = static_cast<uint8_t>(number);
    } else if (strcmp(key, "control_channel_hz") == 0) {
      if (!parse_uint(field, &number) || number < kP25MinHz || number > kP25MaxHz ||
          has_channel(parsed, number) || parsed.control_channel_count >= kMaxControlChannels) {
        snprintf(error, error_size, "line %lu control channel", static_cast<unsigned long>(line_number));
        return false;
      }
      parsed.control_channels_hz[parsed.control_channel_count++] = number;
    } else if (strcmp(key, "last_control_channel_hz") == 0) {
      if (!parse_uint(field, &number)) {
        snprintf(error, error_size, "line %lu last channel", static_cast<unsigned long>(line_number));
        return false;
      }
      parsed.last_control_channel_hz = number;
    } else if (strcmp(key, "auto_follow") == 0) {
      if (!parse_bool(field, &parsed.auto_follow)) {
        snprintf(error, error_size, "line %lu auto_follow", static_cast<unsigned long>(line_number));
        return false;
      }
    } else if (strcmp(key, "encryption_skip") == 0) {
      if (!parse_bool(field, &parsed.encryption_skip)) {
        snprintf(error, error_size, "line %lu encryption_skip", static_cast<unsigned long>(line_number));
        return false;
      }
    } else if (strcmp(key, "modulation") == 0) {
      if (strcmp(field, "auto") == 0) parsed.modulation = p25core::Modulation::auto_detect;
      else if (strcmp(field, "c4fm") == 0) parsed.modulation = p25core::Modulation::c4fm;
      else if (strcmp(field, "cqpsk") == 0) parsed.modulation = p25core::Modulation::cqpsk;
      else {
        snprintf(error, error_size, "line %lu modulation", static_cast<unsigned long>(line_number));
        return false;
      }
    } else if (strcmp(key, "cqpsk_timing_gain") == 0) {
      if (!parse_float(field, &parsed.cqpsk_timing_gain)) {
        snprintf(error, error_size, "line %lu timing gain", static_cast<unsigned long>(line_number));
        return false;
      }
    } else if (strcmp(key, "cqpsk_carrier_gain") == 0) {
      if (!parse_float(field, &parsed.cqpsk_carrier_gain)) {
        snprintf(error, error_size, "line %lu carrier gain", static_cast<unsigned long>(line_number));
        return false;
      }
    } else if (strcmp(key, "hold_talkgroup") == 0) {
      if (!parse_uint(field, &number) || number > UINT16_MAX) {
        snprintf(error, error_size, "line %lu hold_talkgroup", static_cast<unsigned long>(line_number));
        return false;
      }
      parsed.hold_talkgroup = static_cast<uint16_t>(number);
    } else if (strcmp(key, "talkgroup") == 0) {
      char* comma = strchr(field, ',');
      if (comma == nullptr) {
        snprintf(error, error_size, "line %lu talkgroup", static_cast<unsigned long>(line_number));
        return false;
      }
      *comma = '\0';
      char* alias = trim(comma + 1);
      if (!parse_uint(trim(field), &number) || number == 0 || number > UINT16_MAX ||
          *alias == '\0' || strlen(alias) >= sizeof(parsed.talkgroups[0].alias) ||
          has_talkgroup(parsed, static_cast<uint16_t>(number)) ||
          parsed.talkgroup_count >= kMaxTalkgroups) {
        snprintf(error, error_size, "line %lu talkgroup", static_cast<unsigned long>(line_number));
        return false;
      }
      Talkgroup& talkgroup = parsed.talkgroups[parsed.talkgroup_count++];
      talkgroup.id = static_cast<uint16_t>(number);
      snprintf(talkgroup.alias, sizeof(talkgroup.alias), "%s", alias);
    } else {
      snprintf(error, error_size, "line %lu unknown key", static_cast<unsigned long>(line_number));
      return false;
    }
  }
  if (!has_version) {
    set_error(error, error_size, "missing version");
    return false;
  }
  if (!validate(parsed, error, error_size)) return false;
  *config = parsed;
  return true;
}

}  // namespace

bool parse(const char* text, Config* config, char* error, size_t error_size) {
  if (text == nullptr || config == nullptr) {
    set_error(error, error_size, "invalid parser input");
    return false;
  }
  return parse_text_impl(text, config, error, error_size);
}

void defaults(Config* config) {
  if (config == nullptr) return;
  *config = {};
  config->version = kSchemaVersion;
  config->modulation = p25core::Modulation::auto_detect;
  config->cqpsk_timing_gain = 0.005f;
  config->cqpsk_carrier_gain = 0.008f;
  snprintf(config->system_name, sizeof(config->system_name), "%s", "No P25 system configured");
}

bool validate(const Config& config, char* error, size_t error_size) {
  if (config.version != kSchemaVersion || config.system_name[0] == '\0' ||
      config.control_channel_count == 0 || config.control_channel_count > kMaxControlChannels ||
      config.talkgroup_count > kMaxTalkgroups) {
    set_error(error, error_size, "invalid profile header");
    return false;
  }
  if (!std::isfinite(config.cqpsk_timing_gain) || config.cqpsk_timing_gain < 0.0001f ||
      config.cqpsk_timing_gain > 0.05f || !std::isfinite(config.cqpsk_carrier_gain) ||
      config.cqpsk_carrier_gain < 0.0001f || config.cqpsk_carrier_gain > 0.1f) {
    set_error(error, error_size, "CQPSK loop gain");
    return false;
  }
  for (size_t i = 0; i < config.control_channel_count; ++i) {
    const uint32_t frequency_hz = config.control_channels_hz[i];
    if (frequency_hz < kP25MinHz || frequency_hz > kP25MaxHz) {
      set_error(error, error_size, "control channel range");
      return false;
    }
    for (size_t j = i + 1; j < config.control_channel_count; ++j)
      if (frequency_hz == config.control_channels_hz[j]) {
        set_error(error, error_size, "duplicate control channel");
        return false;
      }
  }
  if (config.last_control_channel_hz != 0 && !has_channel(config, config.last_control_channel_hz)) {
    set_error(error, error_size, "last channel not configured");
    return false;
  }
  for (size_t i = 0; i < config.talkgroup_count; ++i) {
    if (config.talkgroups[i].id == 0 || config.talkgroups[i].alias[0] == '\0') {
      set_error(error, error_size, "invalid talkgroup");
      return false;
    }
    for (size_t j = i + 1; j < config.talkgroup_count; ++j)
      if (config.talkgroups[i].id == config.talkgroups[j].id) {
        set_error(error, error_size, "duplicate talkgroup");
        return false;
      }
  }
  return true;
}

LoadResult load(orcsdr::storage::FileSystem& fs, const char* path, Config* config, char* error, size_t error_size) {
  if (config == nullptr || path == nullptr) return LoadResult::io_error;
  if (!fs.exists(path)) return LoadResult::missing;
  File file = fs.open(path, FILE_READ);
  if (!file) {
    set_error(error, error_size, "cannot open config");
    return LoadResult::io_error;
  }
  char text[2048]{};
  const size_t bytes = file.readBytes(text, sizeof(text) - 1);
  text[bytes] = '\0';
  const bool truncated = file.available();
  file.close();
  if (truncated) {
    set_error(error, error_size, "config too large");
    return LoadResult::invalid;
  }
  return parse(text, config, error, error_size) ? LoadResult::ok : LoadResult::invalid;
}

bool save(orcsdr::storage::FileSystem& fs, const char* path, const Config& config,
          char* error, size_t error_size) {
  if (path == nullptr || path[0] != '/') {
    set_error(error, error_size, "invalid destination");
    return false;
  }
  if (!validate(config, error, error_size)) return false;
  char temporary[128]{};
  char backup[128]{};
  snprintf(temporary, sizeof(temporary), "%s.part", path);
  snprintf(backup, sizeof(backup), "%s.bak", path);
  fs.mkdir("/orcsdr");
  fs.remove(temporary);
  File file = fs.open(temporary, FILE_WRITE, true);
  if (!file) {
    set_error(error, error_size, "cannot create config");
    return false;
  }
  file.printf("# OrcSDR P25 profile — edit on a computer, then reload from Systems.\n");
  file.printf("version=%lu\n", static_cast<unsigned long>(config.version));
  file.printf("system_name=%s\n", config.system_name);
  if (config.nac) file.printf("nac=%u\n", config.nac);
  if (config.wacn) file.printf("wacn=%lu\n", static_cast<unsigned long>(config.wacn));
  if (config.system_id) file.printf("system_id=%u\n", config.system_id);
  if (config.rfss) file.printf("rfss=%u\n", config.rfss);
  if (config.site) file.printf("site=%u\n", config.site);
  for (size_t i = 0; i < config.control_channel_count; ++i)
    file.printf("control_channel_hz=%lu\n", static_cast<unsigned long>(config.control_channels_hz[i]));
  file.printf("last_control_channel_hz=%lu\n", static_cast<unsigned long>(config.last_control_channel_hz));
  file.printf("auto_follow=%s\n", config.auto_follow ? "true" : "false");
  file.printf("encryption_skip=%s\n", config.encryption_skip ? "true" : "false");
  file.printf("modulation=%s\n", p25core::modulation_name(config.modulation));
  file.printf("cqpsk_timing_gain=%.6f\n", static_cast<double>(config.cqpsk_timing_gain));
  file.printf("cqpsk_carrier_gain=%.6f\n", static_cast<double>(config.cqpsk_carrier_gain));
  file.printf("hold_talkgroup=%u\n", config.hold_talkgroup);
  for (size_t i = 0; i < config.talkgroup_count; ++i)
    file.printf("talkgroup=%u,%s\n", config.talkgroups[i].id, config.talkgroups[i].alias);
  file.close();
  Config verified{};
  if (load(fs, temporary, &verified, error, error_size) != LoadResult::ok) {
    fs.remove(temporary);
    return false;
  }
  fs.remove(backup);
  const bool had_target = fs.exists(path);
  if (had_target && !fs.rename(path, backup)) {
    fs.remove(temporary);
    set_error(error, error_size, "cannot back up config");
    return false;
  }
  if (!fs.rename(temporary, path)) {
    if (had_target) fs.rename(backup, path);
    fs.remove(temporary);
    set_error(error, error_size, "cannot replace config");
    return false;
  }
  return true;
}

bool valid_profile_id(const char* id) {
  if (id == nullptr) return false;
  const size_t length = strlen(id);
  if (length == 0 || length >= 32) return false;
  for (size_t i = 0; i < length; ++i)
    if (!(isalnum(static_cast<unsigned char>(id[i])) || id[i] == '-' || id[i] == '_'))
      return false;
  return strcmp(id, ".") != 0 && strcmp(id, "..") != 0;
}

bool refresh(orcsdr::storage::FileSystem& fs, StoreState* state, char* error,
             size_t error_size) {
  if (state == nullptr) return false;
  *state = {};
  File active = fs.open(kActivePath, FILE_READ);
  if (active) {
    const size_t bytes = active.readBytes(state->active_id, sizeof(state->active_id) - 1);
    state->active_id[bytes] = '\0';
    char* newline = strpbrk(state->active_id, "\r\n");
    if (newline) *newline = '\0';
    if (!valid_profile_id(state->active_id)) state->active_id[0] = '\0';
  }
  File root = fs.open(kProfilesRoot, FILE_READ);
  if (!root || !root.isDirectory()) return true;
  while (state->count < kMaxProfiles) {
    File entry = root.openNextFile();
    if (!entry) break;
    if (!entry.isDirectory()) continue;
    const char* name = strrchr(entry.name(), '/');
    const char* id = name ? name + 1 : entry.name();
    if (!valid_profile_id(id)) continue;
    char path[96]{};
    Config config{};
    if (!profile_path(id, path, sizeof(path))) continue;
    if (load(fs, path, &config, error, error_size) != LoadResult::ok) {
      char backup[112]{};
      snprintf(backup, sizeof(backup), "%s.bak", path);
      if (load(fs, backup, &config, error, error_size) != LoadResult::ok) continue;
    }
    ProfileSummary& summary = state->profiles[state->count++];
    snprintf(summary.id, sizeof(summary.id), "%s", id);
    snprintf(summary.name, sizeof(summary.name), "%s", config.system_name);
  }
  std::sort(std::begin(state->profiles), std::begin(state->profiles) + state->count,
            [](const ProfileSummary& a, const ProfileSummary& b) {
              return strcmp(a.name, b.name) < 0;
            });
  for (size_t i = 0; i < state->count; ++i)
    if (strcmp(state->profiles[i].id, state->active_id) == 0)
      state->active_index = static_cast<int8_t>(i);
  return true;
}

bool select(orcsdr::storage::FileSystem& fs, const char* id, Config* config,
            StoreState* state, char* error, size_t error_size) {
  char path[96]{};
  if (!profile_path(id, path, sizeof(path))) return false;
  if (load(fs, path, config, error, error_size) != LoadResult::ok) {
    char backup[112]{};
    snprintf(backup, sizeof(backup), "%s.bak", path);
    if (load(fs, backup, config, error, error_size) != LoadResult::ok) return false;
    fs.remove(path);
    if (!save(fs, path, *config, error, error_size)) return false;
  }
  if (!write_active(fs, id)) {
    set_error(error, error_size, "cannot save active profile");
    return false;
  }
  return refresh(fs, state, error, error_size);
}

bool import_profile(orcsdr::storage::FileSystem& fs, const char* source_path,
                    const char* requested_id, StoreState* state, char* error,
                    size_t error_size) {
  if (!valid_profile_id(requested_id)) {
    set_error(error, error_size, "invalid profile id");
    return false;
  }
  if (strncmp(requested_id, "p25_", 4) == 0) {
    set_error(error, error_size, "p25_ ids are reserved for catalog packs");
    return false;
  }
  Config config{};
  if (load(fs, source_path, &config, error, error_size) != LoadResult::ok) return false;
  StoreState current{};
  if (!refresh(fs, &current, error, error_size)) return false;
  if (current.count >= kMaxProfiles) {
    set_error(error, error_size, "profile limit reached");
    return false;
  }
  if (config.wacn != 0 && config.system_id != 0) {
    for (size_t i = 0; i < current.count; ++i) {
      char existing_path[96]{};
      Config existing{};
      if (profile_path(current.profiles[i].id, existing_path, sizeof(existing_path)) &&
          load(fs, existing_path, &existing, error, error_size) == LoadResult::ok &&
          existing.wacn == config.wacn && existing.system_id == config.system_id &&
          existing.rfss == config.rfss && existing.site == config.site) {
        set_error(error, error_size, "system identity already installed");
        return false;
      }
    }
  }
  char directory[80]{}, path[96]{};
  snprintf(directory, sizeof(directory), "%s/%s", kProfilesRoot, requested_id);
  if (!profile_path(requested_id, path, sizeof(path))) return false;
  if (fs.exists(path)) {
    set_error(error, error_size, "profile id already exists");
    return false;
  }
  if (!fs.mkdir("/orcsdr") || !fs.mkdir(kProfilesRoot) || !fs.mkdir(directory) ||
      !save(fs, path, config, error, error_size)) return false;
  if (!write_active(fs, requested_id)) {
    set_error(error, error_size, "cannot save active profile");
    return false;
  }
  return refresh(fs, state, error, error_size);
}

bool export_profile(orcsdr::storage::FileSystem& fs, const char* id,
                    const char* destination_path, char* error, size_t error_size) {
  constexpr size_t prefix_length = sizeof(kExportsRoot) - 1;
  if (destination_path == nullptr ||
      strncmp(destination_path, kExportsRoot, prefix_length) != 0 ||
      destination_path[prefix_length] != '/' ||
      destination_path[prefix_length + 1] == '\0' ||
      strchr(destination_path + prefix_length + 1, '/') != nullptr ||
      strstr(destination_path, "..") != nullptr) {
    set_error(error, error_size, "export path must be /orcsdr/exports/<file>");
    return false;
  }
  char path[96]{};
  Config config{};
  return fs.mkdir(kExportsRoot) && profile_path(id, path, sizeof(path)) &&
         load(fs, path, &config, error, error_size) == LoadResult::ok &&
         save(fs, destination_path, config, error, error_size);
}

bool rename_profile(orcsdr::storage::FileSystem& fs, const char* id,
                    const char* name, StoreState* state, char* error,
                    size_t error_size) {
  if (name == nullptr || name[0] == '\0' || strlen(name) >= 48) {
    set_error(error, error_size, "invalid system name");
    return false;
  }
  char path[96]{};
  Config config{};
  if (!profile_path(id, path, sizeof(path)) ||
      load(fs, path, &config, error, error_size) != LoadResult::ok) return false;
  snprintf(config.system_name, sizeof(config.system_name), "%s", name);
  return save(fs, path, config, error, error_size) &&
         refresh(fs, state, error, error_size);
}

bool delete_profile(orcsdr::storage::FileSystem& fs, const char* id,
                    StoreState* state, char* error, size_t error_size) {
  char path[96]{}, backup[112]{}, temporary[112]{};
  if (!profile_path(id, path, sizeof(path))) {
    set_error(error, error_size, "cannot delete profile");
    return false;
  }
  snprintf(backup, sizeof(backup), "%s.bak", path);
  snprintf(temporary, sizeof(temporary), "%s.part", path);
  const bool found = fs.exists(path) || fs.exists(backup) || fs.exists(temporary);
  const bool removed = (!fs.exists(path) || fs.remove(path)) &&
                       (!fs.exists(backup) || fs.remove(backup)) &&
                       (!fs.exists(temporary) || fs.remove(temporary));
  if (!found || !removed) {
    set_error(error, error_size, "cannot delete profile");
    return false;
  }
  if (state && strcmp(state->active_id, id) == 0) fs.remove(kActivePath);
  return refresh(fs, state, error, error_size);
}

LoadResult load_active(orcsdr::storage::FileSystem& fs, Config* config,
                       StoreState* state, char* error, size_t error_size) {
  if (!refresh(fs, state, error, error_size)) return LoadResult::io_error;
  if (state->active_index >= 0)
    return select(fs, state->active_id, config, state, error, error_size)
               ? LoadResult::ok : LoadResult::invalid;
  if (state->count > 0)
    return select(fs, state->profiles[0].id, config, state, error, error_size)
               ? LoadResult::ok : LoadResult::invalid;
  if (!fs.exists(kLegacyPath)) return LoadResult::missing;
  if (!import_profile(fs, kLegacyPath, "legacy-import", state, error, error_size))
    return LoadResult::invalid;
  return select(fs, "legacy-import", config, state, error, error_size)
             ? LoadResult::ok : LoadResult::invalid;
}

bool self_check() {
  Config config{};
  defaults(&config);
  char error[48]{};
  if (validate(config, error, sizeof(error)) || config.control_channel_count != 0 ||
      strcmp(config.system_name, "No P25 system configured") != 0) return false;
  constexpr char kEditedProfile[] =
      "version=1\n"
      "system_name=Test System\n"
      "control_channel_hz=460000000\n"
      "last_control_channel_hz=460000000\n"
      "auto_follow=false\n"
      "encryption_skip=true\n"
      "modulation=cqpsk\n"
      "cqpsk_timing_gain=0.004\n"
      "cqpsk_carrier_gain=0.01\n"
      "hold_talkgroup=42\n"
      "talkgroup=42,Dispatch\n";
  if (!parse(kEditedProfile, &config, error, sizeof(error)) ||
      config.control_channel_count != 1 || config.talkgroup_count != 1 ||
      config.auto_follow || config.talkgroups[0].id != 42 ||
      config.modulation != p25core::Modulation::cqpsk ||
      fabsf(config.cqpsk_timing_gain - 0.004f) > 0.00001f ||
      fabsf(config.cqpsk_carrier_gain - 0.01f) > 0.00001f) return false;
  constexpr char kBadProfile[] = "version=1\ncontrol_channel_hz=100\n";
  if (parse(kBadProfile, &config, error, sizeof(error))) return false;
  if (!valid_profile_id("wacn-12345_sys-123") || valid_profile_id("../bad")) return false;
  config.control_channels_hz[1] = config.control_channels_hz[0];
  config.control_channel_count = 2;
  return !validate(config, error, sizeof(error));
}

}  // namespace orcsdr::p25config
