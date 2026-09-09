#pragma once

#include <cstdint>

namespace orcsdr::theme {

// Shared RGB565 tokens for the native Tab5 UI. Dashboards may add
// data-visualization palettes, but navigation, surfaces, and status states
// should use these values so moving between screens feels like one product.
inline constexpr uint16_t background = 0x0000;
inline constexpr uint16_t surface = 0x0841;
inline constexpr uint16_t surface_deep = 0x0021;
inline constexpr uint16_t surface_selected = 0x00a0;
inline constexpr uint16_t primary = 0x05ff;
inline constexpr uint16_t success = 0x6fe0;
inline constexpr uint16_t text = 0xffff;
inline constexpr uint16_t text_muted = 0x8c71;
inline constexpr uint16_t divider = 0x4228;
inline constexpr uint16_t grid = 0x2945;
inline constexpr uint16_t danger = 0xf800;
inline constexpr uint16_t warning = 0xfd20;

inline constexpr int control_radius = 8;
inline constexpr int panel_radius = 10;
inline constexpr int minimum_touch_size = 48;

}  // namespace orcsdr::theme
