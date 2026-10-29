#pragma once
#include "map_downloads.hpp"
#include "screen_controller.hpp"

namespace orcsdr::map_download_notice {
// Shared-header gap: after the brand, before the existing device status bar.
constexpr int x=318,y=12,width=94,height=54;
void invalidate();
void update(const maps::downloads::Snapshot&,screens::Id,bool blocked);
bool hit(int32_t,int32_t,screens::Id,bool blocked);
}
