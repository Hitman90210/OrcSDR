#pragma once

#include <cstddef>
#include <cstdint>

namespace orcsdr::dashboards {

enum class Id : uint8_t {
  home,
  fm,
  p25,
  adsb,
  shortwave,
  weather,
  cb,
  lora,
  airband,
  marine,
  satellite,
  utilities,
  settings,
  rf_lab,
  wifi_analysis,
  pocsag,
  // New ids go on the END, never in the middle: these values are written to
  // NVS by persist_dashboard_open and the dash_recent list, so inserting gmrs
  // next to cb (where it belongs on screen) would have shifted lora through
  // settings by one and made every upgraded device restore the wrong tile.
  // Where a dashboard appears in the grid comes from kEntries' order instead.
  gmrs,
  // Upstream numbers am as 16, because it never had gmrs. This fork shipped
  // gmrs=16 first and devices already hold that value, so am follows at 17 and
  // our numbering diverges from upstream here permanently. Keep this order on
  // every future merge: matching upstream would silently repoint every saved
  // GMRS tile at AM.
  am,
  count,
};

// These two values are in the field, written to NVS as dash_recent entries and
// by persist_dashboard_open. A future upstream merge that takes their ordering
// would repoint every saved GMRS tile at AM, silently and only on upgrade --
// so fail the build instead of shipping it.
static_assert(static_cast<uint8_t>(Id::gmrs) == 16, "gmrs must stay at 16");
static_assert(static_cast<uint8_t>(Id::am) == 17, "am follows gmrs at 17");

enum class Category : uint8_t { audio, digital, aviation, utility, system };

struct Descriptor {
  Id id;
  const char* title;
  const char* subtitle;
  Category category;
  bool available;
};

constexpr size_t kRecentCapacity = 12;

const Descriptor* find(Id id);
const Descriptor* descriptor(size_t index);
size_t count();
void load_recent(const uint8_t* ids, size_t count);
bool record_open(Id id);
size_t recent_count();
Id recent(size_t index);
size_t copy_recent(uint8_t* ids, size_t capacity);
bool self_check();

}  // namespace orcsdr::dashboards
