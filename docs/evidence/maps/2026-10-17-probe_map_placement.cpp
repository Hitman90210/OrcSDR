#include "map_view.hpp"
#include "receiver_location.hpp"
#include <esp_attr.h>
EXT_RAM_BSS_ATTR orcsdr::maps::RegionalView probe_regional;
EXT_RAM_BSS_ATTR orcsdr::maps::FrameInfo probe_frame;
EXT_RAM_BSS_ATTR orcsdr::receiver_location::Snapshot probe_location{};
EXT_RAM_BSS_ATTR char probe_identity[256]{};
