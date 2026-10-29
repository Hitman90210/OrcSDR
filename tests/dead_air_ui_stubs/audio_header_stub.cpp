#include "dashboard_audio_control.hpp"
namespace orcsdr::audio_header {
void draw_brand(const char*) {}
void draw_battery(int32_t) {}
void draw_home_button() {}
bool home_hit(int32_t x, int32_t y) { return x >= 1040 && x < 1098 && y >= 8 && y < 66; }
void draw_mute_button(bool) {}
bool mute_hit(int32_t x, int32_t y) { return x >= 1099 && x < 1153 && y >= 12 && y < 66; }
void draw_visualizer_button(bool) {}
void draw_settings_button() {}
bool settings_hit(int32_t x, int32_t y) { return x >= 1217 && x < 1268 && y >= 12 && y < 66; }
}
